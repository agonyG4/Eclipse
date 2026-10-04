use crate::animation::state::{AnimationConfiguration, AnimationSnapshot};
use crate::typhon::material_program::{
    MaterialProgramConfiguration, MaterialProgramDescriptionSnapshot, MaterialProgramNameArguments,
    MaterialProgramParameterConfiguration, MaterialProgramParameterConfigurationSnapshot,
    MaterialProgramStateSnapshot,
};
use crate::typhon::output::{
    OutputConfigureRequest, OutputListSnapshot, OutputRequest, OutputTransactionRequest,
};
use crate::visual_effects::{MaterialConfiguration, MaterialSnapshot};
use serde::Serialize;
use serde_json::Number;

pub const MAX_REQUEST_BYTES: usize = 64 * 1024;
pub const MAX_RESPONSE_BYTES: usize = 1024 * 1024;
const PROTOCOL: &str = "astrea.control";

#[derive(Clone, Debug, PartialEq)]
pub enum AnimationRequest {
    Get,
    Set(AnimationConfiguration),
}

#[derive(Clone, Debug, PartialEq)]
pub enum MaterialRequest {
    Get,
    Set(MaterialConfiguration),
}

#[derive(Clone, Debug, PartialEq)]
pub enum MaterialProgramRequest {
    StateGet,
    Describe(MaterialProgramNameArguments),
    Set(MaterialProgramConfiguration),
    ParametersSet(MaterialProgramParameterConfiguration),
}

#[derive(Clone, Debug, PartialEq)]
pub enum ControlRequest {
    Animation(AnimationRequest),
    Material(MaterialRequest),
    MaterialProgram(MaterialProgramRequest),
    Output(OutputRequest),
}

impl From<AnimationRequest> for ControlRequest {
    fn from(request: AnimationRequest) -> Self {
        Self::Animation(request)
    }
}

impl From<OutputRequest> for ControlRequest {
    fn from(request: OutputRequest) -> Self {
        Self::Output(request)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ProtocolError {
    RequestTooLarge,
    ResponseTooLarge,
    InvalidFraming,
    InvalidJson,
    Incompatible,
    MismatchedId,
    InvalidSuccessFlag,
    MissingResult,
    MissingError,
}

#[derive(Clone, Debug, PartialEq)]
pub enum ControlSuccess {
    Animation(AnimationSnapshot),
    Material(MaterialSnapshot),
    MaterialProgramState(MaterialProgramStateSnapshot),
    MaterialProgramDescription(MaterialProgramDescriptionSnapshot),
    MaterialProgramSelection(crate::typhon::material_program::MaterialProgramSelectionSnapshot),
    MaterialProgramParameters(MaterialProgramParameterConfigurationSnapshot),
    Outputs(OutputListSnapshot),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ControlRejection {
    pub code: Option<String>,
    pub message: String,
    pub detail: Option<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum ProtocolOutcome {
    Success(ControlSuccess),
    ServerRejected(ControlRejection),
}

#[derive(Serialize)]
struct WireRequest {
    protocol: &'static str,
    version: u8,
    id: u64,
    command: &'static str,
    args: WireArguments,
}

#[derive(Serialize)]
#[serde(untagged)]
enum WireArguments {
    Get(GetArguments),
    AnimationSet(AnimationSetArguments),
    MaterialSet(MaterialConfiguration),
    MaterialProgramName(MaterialProgramNameArguments),
    MaterialProgramSelection(MaterialProgramConfiguration),
    MaterialProgramParameters(MaterialProgramParameterConfiguration),
    OutputGet(GetArguments),
    OutputConfigure(OutputConfigureRequest),
    OutputTransaction(OutputTransactionRequest),
}

#[derive(Serialize)]
struct GetArguments {}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct AnimationSetArguments {
    version: u8,
    enabled: bool,
    animate_maximized_window_open: bool,
    preset: String,
    speed: f64,
    overrides: std::collections::BTreeMap<String, String>,
}

pub fn encode_request(id: u64, request: AnimationRequest) -> Result<Vec<u8>, ProtocolError> {
    encode_control_request(id, ControlRequest::Animation(request))
}

pub fn encode_control_request(id: u64, request: ControlRequest) -> Result<Vec<u8>, ProtocolError> {
    let (command, args) = match request {
        ControlRequest::Animation(AnimationRequest::Get) => {
            ("animation.config.get", WireArguments::Get(GetArguments {}))
        }
        ControlRequest::Animation(AnimationRequest::Set(configuration)) => (
            "animation.config.set",
            WireArguments::AnimationSet(AnimationSetArguments {
                version: 1,
                enabled: configuration.enabled,
                animate_maximized_window_open: configuration.animate_maximized_window_open,
                preset: configuration.preset,
                speed: configuration.speed,
                overrides: configuration.overrides,
            }),
        ),
        ControlRequest::Material(MaterialRequest::Get) => {
            ("material.config.get", WireArguments::Get(GetArguments {}))
        }
        ControlRequest::Material(MaterialRequest::Set(configuration)) => (
            "material.config.set",
            WireArguments::MaterialSet(configuration),
        ),
        ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet) => (
            "material.program.state.get",
            WireArguments::Get(GetArguments {}),
        ),
        ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(arguments)) => (
            "material.program.describe",
            WireArguments::MaterialProgramName(arguments),
        ),
        ControlRequest::MaterialProgram(MaterialProgramRequest::Set(configuration)) => (
            "material.program.set",
            WireArguments::MaterialProgramSelection(configuration),
        ),
        ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(configuration)) => (
            "material.program.parameters.set",
            WireArguments::MaterialProgramParameters(configuration),
        ),
        ControlRequest::Output(OutputRequest::Get) => {
            ("outputs", WireArguments::OutputGet(GetArguments {}))
        }
        ControlRequest::Output(OutputRequest::Configure(configuration)) => {
            configuration
                .validate()
                .map_err(|_| ProtocolError::InvalidJson)?;
            (
                "outputs.configure",
                WireArguments::OutputConfigure(configuration),
            )
        }
        ControlRequest::Output(OutputRequest::Confirm(transaction)) => {
            transaction
                .validate()
                .map_err(|_| ProtocolError::InvalidJson)?;
            (
                "outputs.configure.confirm",
                WireArguments::OutputTransaction(transaction),
            )
        }
        ControlRequest::Output(OutputRequest::Revert(transaction)) => {
            transaction
                .validate()
                .map_err(|_| ProtocolError::InvalidJson)?;
            (
                "outputs.configure.revert",
                WireArguments::OutputTransaction(transaction),
            )
        }
    };
    let request = WireRequest {
        protocol: PROTOCOL,
        version: 1,
        id,
        command,
        args,
    };
    let mut encoded = serde_json::to_vec(&request).map_err(|_| ProtocolError::InvalidJson)?;
    encoded.push(b'\n');
    if encoded.len() > MAX_REQUEST_BYTES {
        return Err(ProtocolError::RequestTooLarge);
    }
    Ok(encoded)
}

pub fn decode_response(bytes: &[u8], expected_id: u64) -> Result<ProtocolOutcome, ProtocolError> {
    decode_control_response(
        bytes,
        expected_id,
        &ControlRequest::Animation(AnimationRequest::Get),
    )
}

pub fn decode_control_response(
    bytes: &[u8],
    expected_id: u64,
    request: &ControlRequest,
) -> Result<ProtocolOutcome, ProtocolError> {
    if bytes.len() > MAX_RESPONSE_BYTES {
        return Err(ProtocolError::ResponseTooLarge);
    }
    let Some(newline) = bytes.iter().position(|byte| *byte == b'\n') else {
        return Err(ProtocolError::InvalidFraming);
    };
    if newline == 0
        || bytes
            .get(newline + 1..)
            .is_some_and(|tail| !tail.is_empty())
    {
        return Err(ProtocolError::InvalidFraming);
    }
    let value: serde_json::Value =
        serde_json::from_slice(&bytes[..newline]).map_err(|_| ProtocolError::InvalidJson)?;
    let Some(object) = value.as_object() else {
        return Err(ProtocolError::InvalidJson);
    };
    let protocol = object.get("protocol").and_then(serde_json::Value::as_str);
    let version = object.get("version").and_then(serde_json::Value::as_f64);
    if protocol != Some(PROTOCOL) || version != Some(1.0) {
        return Err(ProtocolError::Incompatible);
    }
    let response_id = object
        .get("id")
        .and_then(serde_json::Value::as_number)
        .and_then(integral_id);
    if response_id != Some(expected_id) {
        return Err(ProtocolError::MismatchedId);
    }
    let Some(ok) = object.get("ok") else {
        return Err(ProtocolError::InvalidSuccessFlag);
    };
    let Some(ok) = ok.as_bool() else {
        return Err(ProtocolError::InvalidSuccessFlag);
    };
    if ok {
        let Some(result) = object.get("result").filter(|result| result.is_object()) else {
            return Err(ProtocolError::MissingResult);
        };
        let success = match request {
            ControlRequest::Animation(_) => ControlSuccess::Animation(
                serde_json::from_value(result.clone()).map_err(|_| ProtocolError::InvalidJson)?,
            ),
            ControlRequest::Material(_) => {
                let snapshot: MaterialSnapshot = serde_json::from_value(result.clone())
                    .map_err(|_| ProtocolError::InvalidJson)?;
                snapshot
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::Material(snapshot)
            }
            ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet) => {
                let snapshot: MaterialProgramStateSnapshot = serde_json::from_value(result.clone())
                    .map_err(|_| ProtocolError::InvalidJson)?;
                snapshot
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::MaterialProgramState(snapshot)
            }
            ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(_)) => {
                let description: MaterialProgramDescriptionSnapshot =
                    serde_json::from_value(result.clone())
                        .map_err(|_| ProtocolError::InvalidJson)?;
                description
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::MaterialProgramDescription(description)
            }
            ControlRequest::MaterialProgram(MaterialProgramRequest::Set(_)) => {
                let selection: crate::typhon::material_program::MaterialProgramSelectionSnapshot =
                    serde_json::from_value(result.clone())
                        .map_err(|_| ProtocolError::InvalidJson)?;
                selection
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::MaterialProgramSelection(selection)
            }
            ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(_)) => {
                let snapshot: MaterialProgramParameterConfigurationSnapshot =
                    serde_json::from_value(result.clone())
                        .map_err(|_| ProtocolError::InvalidJson)?;
                snapshot
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::MaterialProgramParameters(snapshot)
            }
            ControlRequest::Output(_) => {
                let snapshot: OutputListSnapshot = serde_json::from_value(result.clone())
                    .map_err(|_| ProtocolError::InvalidJson)?;
                snapshot
                    .validate()
                    .map_err(|_| ProtocolError::InvalidJson)?;
                ControlSuccess::Outputs(snapshot)
            }
        };
        Ok(ProtocolOutcome::Success(success))
    } else {
        let Some(error) = object.get("error").and_then(serde_json::Value::as_object) else {
            return Err(ProtocolError::MissingError);
        };
        let code = optional_rejection_string(error, "code")?;
        let detail = optional_rejection_string(error, "detail")?;
        let message = error
            .get("message")
            .and_then(serde_json::Value::as_str)
            .filter(|message| !message.is_empty())
            .map(str::to_owned)
            .unwrap_or_else(|| String::from("Typhon control: server rejected request"));
        Ok(ProtocolOutcome::ServerRejected(ControlRejection {
            code,
            message,
            detail,
        }))
    }
}

fn optional_rejection_string(
    error: &serde_json::Map<String, serde_json::Value>,
    field: &str,
) -> Result<Option<String>, ProtocolError> {
    match error.get(field) {
        None | Some(serde_json::Value::Null) => Ok(None),
        Some(serde_json::Value::String(value)) => Ok(Some(value.clone())),
        Some(_) => Err(ProtocolError::InvalidJson),
    }
}

fn integral_id(number: &Number) -> Option<u64> {
    if let Some(value) = number.as_u64() {
        return Some(value);
    }
    let value = number.as_f64()?;
    (value.is_finite() && value >= 0.0 && value.trunc() == value).then_some(value as u64)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::animation::state::AnimationConfiguration;
    use crate::typhon::material_program::{
        MaterialProgramFallbackReason, MaterialProgramParameterValue,
    };
    use serde_json::{Value, json};

    #[test]
    fn request_has_astrea_control_protocol_version_one_and_newline() {
        let encoded = encode_request(7, AnimationRequest::Get).unwrap();
        assert_eq!(encoded.last(), Some(&b'\n'));
        let request: Value = serde_json::from_slice(&encoded[..encoded.len() - 1]).unwrap();
        assert_eq!(request["protocol"], "astrea.control");
        assert_eq!(request["version"], 1);
        assert_eq!(request["id"], 7);
        assert_eq!(request["command"], "animation.config.get");
        assert_eq!(request["args"], json!({}));
    }

    #[test]
    fn outputs_get_request_uses_the_existing_control_transport_shape() {
        let encoded =
            encode_control_request(9, crate::typhon::output::OutputRequest::Get.into()).unwrap();

        assert_eq!(
            encoded,
            br#"{"protocol":"astrea.control","version":1,"id":9,"command":"outputs","args":{}}"#
                .iter()
                .copied()
                .chain(*b"\n")
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn outputs_configure_request_encodes_exact_generation_qualified_mode_identity() {
        use crate::typhon::output::{OutputConfigureRequest, OutputRequest, OutputTransform};

        let request = ControlRequest::Output(OutputRequest::Configure(OutputConfigureRequest {
            version: 1,
            output_id: String::from("output-1"),
            base_configuration_generation: 42,
            mode_id: 7,
            scale_milli: 1000,
            transform: OutputTransform::Normal,
        }));
        let encoded = encode_control_request(11, request).unwrap();
        let value: Value = serde_json::from_slice(&encoded[..encoded.len() - 1]).unwrap();

        assert_eq!(value["protocol"], "astrea.control");
        assert_eq!(value["command"], "outputs.configure");
        assert_eq!(value["args"]["version"], 1);
        assert_eq!(value["args"]["outputId"], "output-1");
        assert_eq!(value["args"]["baseConfigurationGeneration"], 42);
        assert_eq!(value["args"]["modeId"], 7);
        assert_eq!(value["args"]["scaleMilli"], 1000);
        assert_eq!(value["args"]["transform"], "normal");
        assert_eq!(encoded.last(), Some(&b'\n'));
    }

    #[test]
    fn outputs_confirm_and_revert_requests_encode_only_the_transaction_identity() {
        use crate::typhon::output::{OutputRequest, OutputTransactionRequest};

        for (request, command) in [
            (
                OutputRequest::Confirm(OutputTransactionRequest {
                    version: 1,
                    transaction_id: 19,
                }),
                "outputs.configure.confirm",
            ),
            (
                OutputRequest::Revert(OutputTransactionRequest {
                    version: 1,
                    transaction_id: 19,
                }),
                "outputs.configure.revert",
            ),
        ] {
            let encoded = encode_control_request(12, ControlRequest::Output(request)).unwrap();
            let value: Value = serde_json::from_slice(&encoded[..encoded.len() - 1]).unwrap();
            assert_eq!(value["protocol"], "astrea.control");
            assert_eq!(value["command"], command);
            assert_eq!(value["args"], json!({"version": 1, "transactionId": 19}));
            assert_eq!(encoded.last(), Some(&b'\n'));
        }
    }

    #[test]
    fn outputs_get_decodes_a_realistic_typed_snapshot() {
        let request = ControlRequest::Output(crate::typhon::output::OutputRequest::Get);
        let result = json!({
            "outputs": [{
                "id": "output-1",
                "name": "DP-1",
                "make": null,
                "model": null,
                "serial": null,
                "enabled": true,
                "currentMode": {"id": 2, "width": 1920, "height": 1080, "refreshMillihz": 165000},
                "physicalSizeMm": {"widthMm": 600, "heightMm": 340},
                "configurationGeneration": 9,
                "scaleMilli": 1000,
                "transform": "normal",
                "mutationCapabilities": {
                    "modeSelectionSupported": true,
                    "scale": {"supported": false, "minMilli": null, "maxMilli": null, "valuesMilli": []},
                    "transform": {"supported": false, "transforms": []},
                    "enableDisableSupported": false,
                    "topologySupported": false,
                    "vrrMutationSupported": false
                },
                "position": {"x": 0, "y": 0},
                "focused": true,
                "backend": "atomic",
                "vrr": {"state": "available"},
                "directScanout": {"state": "unavailable"},
                "modes": [
                    {"id": 1, "width": 1920, "height": 1080, "refreshMillihz": 60000, "preferred": true, "interlaced": false},
                    {"id": 2, "width": 1920, "height": 1080, "refreshMillihz": 165000, "preferred": false, "interlaced": false},
                    {"id": 3, "width": 1600, "height": 900, "refreshMillihz": 60000, "preferred": false, "interlaced": false}
                ],
                "modesTruncated": false
            }],
            "total": 1,
            "truncated": false,
            "transaction": null
        });
        let outcome = decode_control_response(&program_response(91, result), 91, &request)
            .expect("a valid native output snapshot decodes");
        let ProtocolOutcome::Success(ControlSuccess::Outputs(snapshot)) = outcome else {
            panic!("outputs must decode to the explicit output success type");
        };

        assert_eq!(snapshot.outputs.len(), 1);
        assert_eq!(snapshot.outputs[0].name, "DP-1");
        assert_eq!(snapshot.outputs[0].configuration_generation, 9);
        assert_eq!(
            snapshot.outputs[0]
                .current_mode
                .as_ref()
                .unwrap()
                .refresh_millihz,
            165000
        );
        assert_eq!(snapshot.outputs[0].current_mode.as_ref().unwrap().id, 2);
        assert_eq!(snapshot.outputs[0].modes.len(), 3);
        assert_eq!(snapshot.outputs[0].modes[1].id, 2);
        assert!(
            snapshot.outputs[0]
                .mutation_capabilities
                .mode_selection_supported
        );
        assert!(!snapshot.outputs[0].mutation_capabilities.scale.supported);
        assert!(
            !snapshot.outputs[0]
                .mutation_capabilities
                .transform
                .supported
        );
        assert!(snapshot.outputs[0].modes[0].preferred);
        assert_eq!(
            snapshot.outputs[0]
                .physical_size_mm
                .as_ref()
                .unwrap()
                .width_mm,
            600
        );
    }

    #[test]
    fn active_output_transaction_is_recovered_from_an_ordinary_outputs_refresh() {
        let request = ControlRequest::Output(crate::typhon::output::OutputRequest::Get);
        let mut result = valid_output_list();
        result["transaction"] = json!({
            "id": 72,
            "outputId": "output-1",
            "state": "pending_confirmation",
            "appliedConfigurationGeneration": 9,
            "remainingMs": 7300,
            "errorCode": null
        });
        let outcome = decode_control_response(&program_response(95, result), 95, &request)
            .expect("active transaction is part of a complete output snapshot");
        let ProtocolOutcome::Success(ControlSuccess::Outputs(snapshot)) = outcome else {
            panic!("outputs refresh must decode the authoritative transaction");
        };
        let transaction = snapshot.transaction.unwrap();
        assert_eq!(transaction.id, 72);
        assert_eq!(transaction.applied_configuration_generation, 9);
        assert_eq!(transaction.remaining_ms, 7300);
        assert_eq!(transaction.error_code, None);
    }

    #[test]
    fn output_snapshot_rejects_missing_nullable_fields_and_malformed_transactions() {
        let request = ControlRequest::Output(crate::typhon::output::OutputRequest::Get);
        let mut missing_transaction = valid_output_list();
        missing_transaction
            .as_object_mut()
            .unwrap()
            .remove("transaction");
        let mut missing_scale_bound = valid_output_list();
        missing_scale_bound["outputs"][0]["mutationCapabilities"]["scale"]
            .as_object_mut()
            .unwrap()
            .remove("minMilli");
        let mut missing_current_mode = valid_output_list();
        missing_current_mode["outputs"][0]
            .as_object_mut()
            .unwrap()
            .remove("currentMode");
        let mut unknown_transaction_field = valid_output_list();
        unknown_transaction_field["transaction"] = json!({
            "id": 8, "outputId":"output-1", "state":"pending_confirmation",
            "appliedConfigurationGeneration":9, "remainingMs":5000, "errorCode":null,
            "rollbackMode":"1920x1080"
        });
        let mut excessive_remaining = valid_output_list();
        excessive_remaining["transaction"] = json!({
            "id": 8, "outputId":"output-1", "state":"pending_confirmation",
            "appliedConfigurationGeneration":9, "remainingMs":15001, "errorCode":null
        });
        let mut missing_transaction_error = valid_output_list();
        missing_transaction_error["transaction"] = json!({
            "id": 8, "outputId":"output-1", "state":"pending_confirmation",
            "appliedConfigurationGeneration":9, "remainingMs":5000
        });

        for invalid in [
            missing_transaction,
            missing_scale_bound,
            missing_current_mode,
            unknown_transaction_field,
            excessive_remaining,
            missing_transaction_error,
        ] {
            assert_eq!(
                decode_control_response(&program_response(96, invalid), 96, &request),
                Err(ProtocolError::InvalidJson)
            );
        }
    }

    #[test]
    fn every_output_operation_decodes_to_the_same_complete_authoritative_snapshot_type() {
        use crate::typhon::output::{
            OutputConfigureRequest, OutputRequest, OutputTransactionRequest, OutputTransform,
        };
        let requests = [
            OutputRequest::Get,
            OutputRequest::Configure(OutputConfigureRequest {
                version: 1,
                output_id: "output-1".into(),
                base_configuration_generation: 9,
                mode_id: 2,
                scale_milli: 1000,
                transform: OutputTransform::Normal,
            }),
            OutputRequest::Confirm(OutputTransactionRequest {
                version: 1,
                transaction_id: 72,
            }),
            OutputRequest::Revert(OutputTransactionRequest {
                version: 1,
                transaction_id: 72,
            }),
        ];
        for (index, request) in requests.into_iter().enumerate() {
            let outcome = decode_control_response(
                &program_response(100 + index as u64, valid_output_list()),
                100 + index as u64,
                &ControlRequest::Output(request),
            )
            .expect("each operation returns a complete output snapshot");
            assert!(matches!(
                outcome,
                ProtocolOutcome::Success(ControlSuccess::Outputs(_))
            ));
        }
    }

    #[test]
    fn output_snapshot_validation_rejects_malformed_structure_and_bounds() {
        let request = ControlRequest::Output(crate::typhon::output::OutputRequest::Get);
        let mut duplicate_ids = valid_output_list();
        duplicate_ids["outputs"] = json!([
            valid_output(),
             {"id":"output-1", "name":"HDMI-A-1", "make":null, "model":null,
             "serial":null, "enabled":false, "currentMode":null, "physicalSizeMm":null,
             "configurationGeneration":9, "scaleMilli":1000, "transform":"normal",
             "mutationCapabilities":{"modeSelectionSupported":true,
               "scale":{"supported":false,"minMilli":null,"maxMilli":null,"valuesMilli":[]},
               "transform":{"supported":false,"transforms":[]},
               "enableDisableSupported":false,"topologySupported":false,"vrrMutationSupported":false},
             "position":{"x":0,"y":0},
             "focused":false, "backend":"atomic", "vrr":{"state":"unavailable"},
             "directScanout":{"state":"unavailable"}, "modes":[], "modesTruncated":false}
        ]);
        duplicate_ids["total"] = json!(2);

        let mut invalid_mode_size = valid_output_list();
        invalid_mode_size["outputs"][0]["modes"][0]["width"] = json!(0);
        let mut invalid_refresh = valid_output_list();
        invalid_refresh["outputs"][0]["modes"][0]["refreshMillihz"] = json!(0);
        let mut invalid_scale = valid_output_list();
        invalid_scale["outputs"][0]["scaleMilli"] = json!(0);
        let mut invalid_physical_size = valid_output_list();
        invalid_physical_size["outputs"][0]["physicalSizeMm"]["heightMm"] = json!(0);
        let mut invalid_total = valid_output_list();
        invalid_total["total"] = json!(0);
        let mut missing_modes = valid_output_list();
        missing_modes["outputs"][0]
            .as_object_mut()
            .unwrap()
            .remove("modes");
        let mut oversized_modes = valid_output_list();
        let mode = oversized_modes["outputs"][0]["modes"][0].clone();
        oversized_modes["outputs"][0]["modes"] = Value::Array(
            std::iter::repeat_n(mode, crate::typhon::output::MAX_CONTROL_OUTPUT_MODES + 1)
                .collect(),
        );

        for invalid in [
            duplicate_ids,
            invalid_mode_size,
            invalid_refresh,
            invalid_scale,
            invalid_physical_size,
            invalid_total,
            missing_modes,
            oversized_modes,
        ] {
            assert_eq!(
                decode_control_response(&program_response(92, invalid), 92, &request),
                Err(ProtocolError::InvalidJson)
            );
        }

        let mut disabled_without_mode = valid_output_list();
        disabled_without_mode["outputs"][0]["enabled"] = json!(false);
        disabled_without_mode["outputs"][0]["currentMode"] = Value::Null;
        disabled_without_mode["outputs"][0]["modes"] = json!([]);
        assert!(
            decode_control_response(&program_response(93, disabled_without_mode), 93, &request)
                .is_ok()
        );

        let wrong_result_shape = json!([{"outputs": [], "total": 0, "truncated": false}]);
        assert_eq!(
            decode_control_response(&program_response(94, wrong_result_shape), 94, &request),
            Err(ProtocolError::MissingResult)
        );
    }

    fn valid_output_list() -> Value {
        json!({"outputs": [valid_output()], "total": 1, "truncated": false, "transaction": null})
    }

    #[test]
    fn output_configure_encoding_rejects_invalid_versions_and_missing_identity() {
        use crate::typhon::output::{OutputConfigureRequest, OutputRequest, OutputTransform};
        for (version, output_id, generation, mode_id) in [
            (2, "output-1", 9, 2),
            (1, "", 9, 2),
            (1, "output-1", 0, 2),
            (1, "output-1", 9, 0),
        ] {
            let request =
                ControlRequest::Output(OutputRequest::Configure(OutputConfigureRequest {
                    version,
                    output_id: output_id.into(),
                    base_configuration_generation: generation,
                    mode_id,
                    scale_milli: 1000,
                    transform: OutputTransform::Normal,
                }));
            assert_eq!(
                encode_control_request(13, request),
                Err(ProtocolError::InvalidJson)
            );
        }
    }

    fn valid_output() -> Value {
        json!({
            "id": "output-1", "name": "DP-1", "make": null, "model": null,
            "serial": null, "enabled": true,
            "currentMode": {"id":2,"width":1920,"height":1080,"refreshMillihz":165000},
            "physicalSizeMm": {"widthMm":600,"heightMm":340}, "configurationGeneration":9,
            "scaleMilli":1000, "transform":"normal",
            "mutationCapabilities":{"modeSelectionSupported":true,
              "scale":{"supported":false,"minMilli":null,"maxMilli":null,"valuesMilli":[]},
              "transform":{"supported":false,"transforms":[]},
              "enableDisableSupported":false,"topologySupported":false,"vrrMutationSupported":false},
            "position":{"x":0,"y":0}, "focused":true,
            "backend":"atomic", "vrr":{"state":"available"},
            "directScanout":{"state":"unavailable"},
            "modes":[{"id":1,"width":1920,"height":1080,"refreshMillihz":60000,
                       "preferred":true,"interlaced":false},
                      {"id":2,"width":1920,"height":1080,"refreshMillihz":165000,
                       "preferred":false,"interlaced":false}], "modesTruncated":false
        })
    }

    #[test]
    fn set_request_keeps_typed_configuration_inside_args() {
        let configuration = AnimationConfiguration {
            enabled: false,
            animate_maximized_window_open: false,
            preset: "macos".to_owned(),
            speed: 1.25,
            overrides: [("window.move".to_owned(), "geometry.macos".to_owned())]
                .into_iter()
                .collect(),
        };
        let encoded = encode_request(2, AnimationRequest::Set(configuration)).unwrap();
        let request: Value = serde_json::from_slice(&encoded[..encoded.len() - 1]).unwrap();
        assert_eq!(request["command"], "animation.config.set");
        assert_eq!(request["args"]["version"], 1);
        assert_eq!(request["args"]["enabled"], false);
        assert_eq!(request["args"]["animateMaximizedWindowOpen"], false);
        assert_eq!(
            request["args"]["overrides"]["window.move"],
            "geometry.macos"
        );
    }

    #[test]
    fn animation_get_encoding_remains_byte_for_byte_compatible() {
        let legacy = encode_request(7, AnimationRequest::Get).unwrap();
        let generalized =
            encode_control_request(7, ControlRequest::Animation(AnimationRequest::Get)).unwrap();
        assert_eq!(generalized, legacy);
        assert_eq!(
            generalized,
            br#"{"protocol":"astrea.control","version":1,"id":7,"command":"animation.config.get","args":{}}"#
                .iter()
                .copied()
                .chain(*b"\n")
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn material_get_and_set_use_astrea_control_v1_typed_requests() {
        let get =
            encode_control_request(31, ControlRequest::Material(MaterialRequest::Get)).unwrap();
        let get_value: Value = serde_json::from_slice(&get[..get.len() - 1]).unwrap();
        assert_eq!(get_value["protocol"], "astrea.control");
        assert_eq!(get_value["version"], 1);
        assert_eq!(get_value["id"], 31);
        assert_eq!(get_value["command"], "material.config.get");
        assert_eq!(get_value["args"], json!({}));

        let configuration = crate::visual_effects::MaterialConfiguration {
            position: 0.74,
            overrides: crate::visual_effects::MaterialOverrides {
                blur: Some(0.82),
                ..crate::visual_effects::MaterialOverrides::default()
            },
            ..crate::visual_effects::MaterialConfiguration::default()
        };
        let set = encode_control_request(
            32,
            ControlRequest::Material(MaterialRequest::Set(configuration)),
        )
        .unwrap();
        let set_value: Value = serde_json::from_slice(&set[..set.len() - 1]).unwrap();
        assert_eq!(set_value["command"], "material.config.set");
        assert_eq!(set_value["args"]["version"], 1);
        assert_eq!(set_value["args"]["position"], 0.74);
        assert_eq!(set_value["args"]["overrides"]["blur"], 0.82);
    }

    #[test]
    fn material_program_requests_encode_canonical_commands_and_complete_arguments() {
        let state = encode_control_request(
            51,
            ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet),
        )
        .unwrap();
        let state: Value = serde_json::from_slice(&state[..state.len() - 1]).unwrap();
        assert_eq!(state["command"], "material.program.state.get");
        assert_eq!(state["args"], json!({}));

        let describe = encode_control_request(
            52,
            ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(
                MaterialProgramNameArguments {
                    name: "studio.soft_glow".to_owned(),
                },
            )),
        )
        .unwrap();
        let describe: Value = serde_json::from_slice(&describe[..describe.len() - 1]).unwrap();
        assert_eq!(describe["command"], "material.program.describe");
        assert_eq!(describe["args"], json!({"name": "studio.soft_glow"}));
        assert_eq!(describe["args"].as_object().unwrap().len(), 1);

        let selection = MaterialProgramConfiguration {
            version: 1,
            requested_program: "studio.soft_glow".to_owned(),
        };
        let set = encode_control_request(
            53,
            ControlRequest::MaterialProgram(MaterialProgramRequest::Set(selection)),
        )
        .unwrap();
        let set: Value = serde_json::from_slice(&set[..set.len() - 1]).unwrap();
        assert_eq!(set["command"], "material.program.set");
        assert_eq!(
            set["args"],
            json!({"version": 1, "requestedProgram": "studio.soft_glow"})
        );

        let parameters = MaterialProgramParameterConfiguration {
            version: 1,
            program: "studio.soft_glow".to_owned(),
            schema_signature: 9_007_199_254_741_113,
            overrides: [
                (
                    "intensity".to_owned(),
                    MaterialProgramParameterValue::Float(0.8),
                ),
                (
                    "offset".to_owned(),
                    MaterialProgramParameterValue::Vec3([0.1, 0.2, 0.3]),
                ),
            ]
            .into_iter()
            .collect(),
        };
        let set = encode_control_request(
            54,
            ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(parameters)),
        )
        .unwrap();
        let set: Value = serde_json::from_slice(&set[..set.len() - 1]).unwrap();
        assert_eq!(set["command"], "material.program.parameters.set");
        assert_eq!(set["args"]["version"], 1);
        assert_eq!(set["args"]["program"], "studio.soft_glow");
        assert_eq!(set["args"]["schemaSignature"], 9_007_199_254_741_113u64);
        assert_eq!(
            set["args"]["overrides"]["intensity"],
            json!({"type": "float", "value": 0.8})
        );
        assert_eq!(
            set["args"]["overrides"]["offset"],
            json!({"type": "vec3", "value": [0.1, 0.2, 0.3]})
        );
    }

    #[test]
    fn material_program_state_response_is_atomic_and_strict() {
        let request = ControlRequest::MaterialProgram(MaterialProgramRequest::StateGet);
        let outcome =
            decode_control_response(&program_response(61, valid_program_state()), 61, &request)
                .unwrap();
        let ProtocolOutcome::Success(ControlSuccess::MaterialProgramState(snapshot)) = outcome
        else {
            panic!("state.get must return the combined catalog and selection snapshot");
        };
        assert_eq!(snapshot.catalog.registry_generation, 19);
        assert_eq!(snapshot.selection.registry_generation, 19);
        assert_eq!(
            snapshot.selection.configuration.requested_program,
            "missing.request"
        );
        assert_eq!(
            snapshot.selection.effective_program,
            "system.background_blur"
        );
        assert_eq!(
            snapshot.selection.fallback_reason,
            Some(MaterialProgramFallbackReason::Missing)
        );

        let mut mismatched = valid_program_state();
        mismatched["selection"]["registryGeneration"] = json!(20);
        assert_eq!(
            decode_control_response(&program_response(61, mismatched), 61, &request),
            Err(ProtocolError::InvalidJson)
        );

        let mut unknown_field = valid_program_state();
        unknown_field["catalog"]["rendererId"] = json!(7);
        assert_eq!(
            decode_control_response(&program_response(61, unknown_field), 61, &request),
            Err(ProtocolError::InvalidJson)
        );
    }

    #[test]
    fn material_program_description_decodes_all_typed_parameters_and_ranges() {
        let request = ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(
            MaterialProgramNameArguments {
                name: "studio.soft_glow".to_owned(),
            },
        ));
        let description = json!({
            "registryGeneration": 19,
            "parameterGeneration": 4,
            "name": "studio.soft_glow",
            "origin": "trusted_local",
            "schemaSignature": 9007199254741113u64,
            "parameters": [
                {"name":"amount","parameterType":"float","range":{"type":"float","min":0.0,"max":1.0},"default":{"type":"float","value":0.2},"effective":{"type":"float","value":0.4},"overridden":true},
                {"name":"offset","parameterType":"vec2","range":{"type":"float_components","components":2,"min":[-1.0,-1.0],"max":[1.0,1.0]},"default":{"type":"vec2","value":[0.0,0.0]},"effective":{"type":"vec2","value":[0.2,0.4]},"overridden":false},
                {"name":"tint","parameterType":"vec3","range":null,"default":{"type":"vec3","value":[1.0,1.0,1.0]},"effective":{"type":"vec3","value":[0.8,0.9,1.0]},"overridden":false},
                {"name":"weights","parameterType":"vec4","range":{"type":"float_components","components":4,"min":[0.0,0.0,0.0,0.0],"max":[1.0,1.0,1.0,1.0]},"default":{"type":"vec4","value":[0.1,0.2,0.3,0.4]},"effective":{"type":"vec4","value":[0.1,0.2,0.3,0.4]},"overridden":false},
                {"name":"steps","parameterType":"int","range":{"type":"int","min":1,"max":8},"default":{"type":"int","value":2},"effective":{"type":"int","value":3},"overridden":true}
            ]
        });
        let outcome =
            decode_control_response(&program_response(62, description), 62, &request).unwrap();
        let ProtocolOutcome::Success(ControlSuccess::MaterialProgramDescription(description)) =
            outcome
        else {
            panic!("describe must return typed parameter descriptors");
        };
        assert_eq!(description.parameters.len(), 5);
        assert_eq!(description.schema_signature, 9_007_199_254_741_113);
        assert!(matches!(
            description.parameters[0].default,
            MaterialProgramParameterValue::Float(_)
        ));
        assert!(matches!(
            description.parameters[1].effective,
            MaterialProgramParameterValue::Vec2(_)
        ));
        assert!(matches!(
            description.parameters[2].effective,
            MaterialProgramParameterValue::Vec3(_)
        ));
        assert!(matches!(
            description.parameters[3].effective,
            MaterialProgramParameterValue::Vec4(_)
        ));
        assert!(matches!(
            description.parameters[4].effective,
            MaterialProgramParameterValue::Int(_)
        ));
    }

    #[test]
    fn tagged_values_round_trip_and_reject_malformed_numbers_and_vectors() {
        let values = [
            MaterialProgramParameterValue::Float(0.25),
            MaterialProgramParameterValue::Vec2([0.1, 0.2]),
            MaterialProgramParameterValue::Vec3([0.1, 0.2, 0.3]),
            MaterialProgramParameterValue::Vec4([0.1, 0.2, 0.3, 0.4]),
            MaterialProgramParameterValue::Int(7),
        ];
        for value in values {
            let encoded = serde_json::to_value(value).unwrap();
            let decoded: MaterialProgramParameterValue = serde_json::from_value(encoded).unwrap();
            assert_eq!(decoded, value);
        }
        for malformed in [
            json!({"type":"vec2","value":[0.1]}),
            json!({"type":"vec3","value":[0.1,0.2,0.3,0.4]}),
            json!({"type":"vec4","value":[0.1,0.2,0.3]}),
            json!({"type":"float","value":0.2,"extra":true}),
        ] {
            assert!(serde_json::from_value::<MaterialProgramParameterValue>(malformed).is_err());
        }
        assert!(
            serde_json::from_str::<MaterialProgramParameterValue>(
                r#"{"type":"float","value":1e999}"#
            )
            .is_err()
        );
    }

    #[test]
    fn malformed_parameter_ranges_and_missing_required_nullable_fields_are_rejected() {
        let request = ControlRequest::MaterialProgram(MaterialProgramRequest::Describe(
            MaterialProgramNameArguments {
                name: "studio.soft_glow".to_owned(),
            },
        ));
        for range in [
            json!({"type":"float","min":1.0,"max":0.0}),
            json!({"type":"float","min":0.0,"max":1.0,"extra":true}),
            json!({"type":"float_components","components":1,"min":[0.0],"max":[1.0]}),
            json!({"type":"float_components","components":3,"min":[0.0,0.0],"max":[1.0,1.0]}),
            json!({"type":"int","min":9,"max":2}),
        ] {
            let mut description = valid_program_description();
            description["parameters"][0]["range"] = range;
            assert_eq!(
                decode_control_response(&program_response(63, description), 63, &request),
                Err(ProtocolError::InvalidJson)
            );
        }

        let request = ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(
            valid_parameter_configuration(),
        ));
        let nullable = json!({
            "registryGeneration":19,
            "parameterGeneration":5,
            "configuration": {
                "version":1,
                "program":"studio.soft_glow",
                "schemaSignature":9007199254741113u64,
                "overrides":{}
            },
            "description":null
        });
        assert!(matches!(
            decode_control_response(&program_response(64, nullable.clone()), 64, &request),
            Ok(ProtocolOutcome::Success(
                ControlSuccess::MaterialProgramParameters(_)
            ))
        ));
        let mut missing_description = nullable;
        missing_description
            .as_object_mut()
            .unwrap()
            .remove("description");
        assert_eq!(
            decode_control_response(&program_response(64, missing_description), 64, &request),
            Err(ProtocolError::InvalidJson)
        );
    }

    #[test]
    fn structured_server_rejection_preserves_code_message_and_detail() {
        let response = rejection_response(
            65,
            json!({
                "code":"invalid_argument",
                "message":"material program parameter schema is stale",
                "detail":"stale_material_program_schema"
            }),
        );
        assert_eq!(
            decode_control_response(
                &response,
                65,
                &ControlRequest::MaterialProgram(MaterialProgramRequest::ParametersSet(
                    valid_parameter_configuration()
                ))
            ),
            Ok(ProtocolOutcome::ServerRejected(ControlRejection {
                code: Some("invalid_argument".to_owned()),
                message: "material program parameter schema is stale".to_owned(),
                detail: Some("stale_material_program_schema".to_owned()),
            }))
        );

        let legacy = rejection_response(66, json!({"message":"rejected"}));
        assert_eq!(
            decode_control_response(
                &legacy,
                66,
                &ControlRequest::Animation(AnimationRequest::Get)
            ),
            Ok(ProtocolOutcome::ServerRejected(ControlRejection {
                code: None,
                message: "rejected".to_owned(),
                detail: None,
            }))
        );
    }

    #[test]
    fn material_response_decodes_to_authoritative_typed_snapshot() {
        let response = material_response(json!({
            "generation": 4,
            "source": "runtime",
            "configuration": {
                "version": 1,
                "position": 0.7,
                "overrides": {"blur": 0.9, "saturation": null, "noise": null}
            },
            "effective": {"blur": 0.7, "saturation": 0.9, "noise": 0.1},
            "capabilities": {
                "blurOverride": true,
                "saturationOverride": true,
                "noiseOverride": true
            }
        }));

        let outcome = decode_control_response(
            &response,
            44,
            &ControlRequest::Material(MaterialRequest::Get),
        )
        .unwrap();
        let ProtocolOutcome::Success(ControlSuccess::Material(snapshot)) = outcome else {
            panic!("material get must return a material snapshot");
        };
        assert_eq!(snapshot.generation, 4);
        assert_eq!(snapshot.configuration.position, 0.7);
        assert_eq!(snapshot.configuration.overrides.blur, Some(0.9));
    }

    #[test]
    fn incomplete_material_snapshot_is_rejected_at_the_wire_boundary() {
        let response = material_response(json!({
            "generation": 4,
            "source": "runtime",
            "configuration": {"version": 1, "position": 0.5, "overrides": {}},
            "effective": {"blur": 0.5, "saturation": 0.8, "noise": 0.1}
        }));

        assert_eq!(
            decode_control_response(
                &response,
                44,
                &ControlRequest::Material(MaterialRequest::Get)
            ),
            Err(ProtocolError::InvalidJson)
        );
    }

    #[test]
    fn request_over_64_kib_is_rejected_before_transport() {
        let configuration = AnimationConfiguration {
            overrides: [("payload".to_owned(), "x".repeat(70 * 1024))]
                .into_iter()
                .collect(),
            ..AnimationConfiguration::default()
        };
        assert_eq!(
            encode_request(1, AnimationRequest::Set(configuration)),
            Err(ProtocolError::RequestTooLarge)
        );
    }

    #[test]
    fn response_rejects_invalid_json() {
        assert_eq!(
            decode_response(b"not-json\n", 1),
            Err(ProtocolError::InvalidJson)
        );
    }

    #[test]
    fn response_rejects_wrong_protocol_or_version() {
        for response in [
            response_with(json!({"protocol": "wrong.protocol"})),
            response_with(json!({"version": 2})),
        ] {
            assert_eq!(
                decode_response(&response, 1),
                Err(ProtocolError::Incompatible)
            );
        }
    }

    #[test]
    fn response_rejects_non_integral_or_mismatched_id() {
        for id in [json!(1.5), json!(2), json!(-1)] {
            let response = response_with(json!({"id": id}));
            assert_eq!(
                decode_response(&response, 1),
                Err(ProtocolError::MismatchedId)
            );
        }
    }

    #[test]
    fn response_requires_boolean_ok() {
        let response = response_with(json!({"ok": "yes"}));
        assert_eq!(
            decode_response(&response, 1),
            Err(ProtocolError::InvalidSuccessFlag)
        );
    }

    #[test]
    fn successful_response_requires_object_result() {
        let response = response_without_result(json!({"ok": true}));
        assert_eq!(
            decode_response(&response, 1),
            Err(ProtocolError::MissingResult)
        );
        let response = response_with_invalid_result();
        assert_eq!(
            decode_response(&response, 1),
            Err(ProtocolError::MissingResult)
        );
    }

    #[test]
    fn rejected_response_requires_object_error_and_preserves_message() {
        let response = response_with(json!({"ok": false, "error": {"message": "rejected"}}));
        assert_eq!(
            decode_response(&response, 1),
            Ok(ProtocolOutcome::ServerRejected(ControlRejection {
                code: None,
                message: "rejected".to_owned(),
                detail: None,
            }))
        );
        let response = response_with(json!({"ok": false}));
        assert_eq!(
            decode_response(&response, 1),
            Err(ProtocolError::MissingError)
        );
    }

    #[test]
    fn response_rejects_trailing_frame_or_data() {
        let mut response = response_with(json!({"ok": true, "result": {"config": {}}}));
        response.extend_from_slice(b"extra");
        assert_eq!(
            decode_response(&response, 1),
            Err(ProtocolError::InvalidFraming)
        );
    }

    #[test]
    fn response_over_1_mib_is_rejected() {
        assert_eq!(
            decode_response(&vec![b'x'; MAX_RESPONSE_BYTES + 1], 1),
            Err(ProtocolError::ResponseTooLarge)
        );
    }

    fn response_with(overrides: Value) -> Vec<u8> {
        let mut response = json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": 1,
            "ok": true,
            "result": {"config": {"enabled": true, "preset": "astrea", "speed": 1.0}}
        });
        if let Value::Object(fields) = overrides {
            for (key, value) in fields {
                response[key] = value;
            }
        }
        serde_json::to_vec(&response)
            .unwrap_or_default()
            .into_iter()
            .chain(*b"\n")
            .collect()
    }

    fn response_without_result(overrides: Value) -> Vec<u8> {
        let mut response = json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": 1,
            "ok": true
        });
        if let Value::Object(fields) = overrides {
            for (key, value) in fields {
                response[key] = value;
            }
        }
        serde_json::to_vec(&response)
            .unwrap_or_default()
            .into_iter()
            .chain(*b"\n")
            .collect()
    }

    fn response_with_invalid_result() -> Vec<u8> {
        serde_json::to_vec(&json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": 1,
            "ok": true,
            "result": []
        }))
        .unwrap_or_default()
        .into_iter()
        .chain(*b"\n")
        .collect()
    }

    fn program_response(id: u64, result: Value) -> Vec<u8> {
        serde_json::to_vec(&json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": id,
            "ok": true,
            "result": result,
        }))
        .unwrap()
        .into_iter()
        .chain(*b"\n")
        .collect()
    }

    fn rejection_response(id: u64, error: Value) -> Vec<u8> {
        serde_json::to_vec(&json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": id,
            "ok": false,
            "error": error,
        }))
        .unwrap()
        .into_iter()
        .chain(*b"\n")
        .collect()
    }

    fn valid_program_state() -> Value {
        json!({
            "catalog": {
                "registryGeneration": 19,
                "renderingAvailable": true,
                "programs": [{
                    "name": "system.background_blur",
                    "origin": "system",
                    "schemaSignature": 1,
                    "parameterCount": 0
                }]
            },
            "selection": {
                "generation": 7,
                "source": "runtime",
                "configuration": {"version":1,"requestedProgram":"missing.request"},
                "effectiveProgram": "system.background_blur",
                "registryGeneration": 19,
                "renderingAvailable": true,
                "fallbackReason": "missing"
            }
        })
    }

    fn valid_program_description() -> Value {
        json!({
            "registryGeneration": 19,
            "parameterGeneration": 4,
            "name": "studio.soft_glow",
            "origin": "trusted_local",
            "schemaSignature": 9007199254741113u64,
            "parameters": [{
                "name":"amount",
                "parameterType":"float",
                "range":{"type":"float","min":0.0,"max":1.0},
                "default":{"type":"float","value":0.2},
                "effective":{"type":"float","value":0.4},
                "overridden":true
            }]
        })
    }

    fn valid_parameter_configuration() -> MaterialProgramParameterConfiguration {
        MaterialProgramParameterConfiguration {
            version: 1,
            program: "studio.soft_glow".to_owned(),
            schema_signature: 9_007_199_254_741_113,
            overrides: Default::default(),
        }
    }

    fn material_response(result: Value) -> Vec<u8> {
        serde_json::to_vec(&json!({
            "protocol": "astrea.control",
            "version": 1,
            "id": 44,
            "ok": true,
            "result": result,
        }))
        .unwrap()
        .into_iter()
        .chain(*b"\n")
        .collect()
    }
}
