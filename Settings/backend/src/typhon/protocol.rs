use crate::animation::state::{AnimationConfiguration, AnimationSnapshot};
use serde::{Deserialize, Serialize};
use serde_json::Number;

pub const MAX_REQUEST_BYTES: usize = 64 * 1024;
pub const MAX_RESPONSE_BYTES: usize = 1024 * 1024;
const PROTOCOL: &str = "astrea.control";

#[derive(Clone, Debug, PartialEq)]
pub enum AnimationRequest {
    Get,
    Set(AnimationConfiguration),
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
pub enum ProtocolOutcome {
    Success(AnimationSnapshot),
    ServerRejected(String),
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
    Set(SetArguments),
}

#[derive(Serialize)]
struct GetArguments {}

#[derive(Serialize)]
struct SetArguments {
    version: u8,
    enabled: bool,
    preset: String,
    speed: f64,
    overrides: std::collections::BTreeMap<String, String>,
}

#[derive(Deserialize)]
struct WireResponse {
    protocol: Option<String>,
    version: Option<f64>,
    id: Option<Number>,
    ok: Option<serde_json::Value>,
    result: Option<AnimationSnapshot>,
    error: Option<WireError>,
}

#[derive(Deserialize)]
struct WireError {
    message: Option<String>,
}

pub fn encode_request(id: u64, request: AnimationRequest) -> Result<Vec<u8>, ProtocolError> {
    let (command, args) = match request {
        AnimationRequest::Get => ("animation.config.get", WireArguments::Get(GetArguments {})),
        AnimationRequest::Set(configuration) => (
            "animation.config.set",
            WireArguments::Set(SetArguments {
                version: 1,
                enabled: configuration.enabled,
                preset: configuration.preset,
                speed: configuration.speed,
                overrides: configuration.overrides,
            }),
        ),
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
    if object
        .get("result")
        .is_some_and(|result| !result.is_object() && !result.is_null())
        || object
            .get("error")
            .is_some_and(|error| !error.is_object() && !error.is_null())
    {
        return Err(ProtocolError::InvalidJson);
    }
    let response: WireResponse =
        serde_json::from_value(value).map_err(|_| ProtocolError::InvalidJson)?;
    if response.protocol.as_deref() != Some(PROTOCOL) || response.version != Some(1.0) {
        return Err(ProtocolError::Incompatible);
    }
    if response.id.as_ref().and_then(integral_id) != Some(expected_id) {
        return Err(ProtocolError::MismatchedId);
    }
    let Some(ok) = response.ok else {
        return Err(ProtocolError::InvalidSuccessFlag);
    };
    let Some(ok) = ok.as_bool() else {
        return Err(ProtocolError::InvalidSuccessFlag);
    };
    if ok {
        response
            .result
            .map(ProtocolOutcome::Success)
            .ok_or(ProtocolError::MissingResult)
    } else {
        let Some(error) = response.error else {
            return Err(ProtocolError::MissingError);
        };
        let message = error
            .message
            .filter(|message| !message.is_empty())
            .unwrap_or_else(|| String::from("Typhon control: server rejected request"));
        Ok(ProtocolOutcome::ServerRejected(message))
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
    fn set_request_keeps_typed_configuration_inside_args() {
        let configuration = AnimationConfiguration {
            enabled: false,
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
        assert_eq!(
            request["args"]["overrides"]["window.move"],
            "geometry.macos"
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
            Err(ProtocolError::InvalidJson)
        );
    }

    #[test]
    fn rejected_response_requires_object_error_and_preserves_message() {
        let response = response_with(json!({"ok": false, "error": {"message": "rejected"}}));
        assert_eq!(
            decode_response(&response, 1),
            Ok(ProtocolOutcome::ServerRejected("rejected".to_owned()))
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
}
