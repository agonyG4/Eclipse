use std::time::Duration;

use astrea_system_backend::bluetooth::agent::{
    AgentBroker, AgentError, AgentPromptKind, AgentSubmitResult,
};

const SENDER: &str = ":1.42";
const SESSION: u64 = 7;
const BLUEZ: u64 = 11;
const EPOCH: u64 = 3;
const DEVICE: &str = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";

fn broker() -> AgentBroker {
    let broker = AgentBroker::new_with_prompt_timeout(Duration::from_millis(50));
    broker.set_authority(SESSION, BLUEZ, SENDER);
    broker.set_pairing_context(EPOCH, SESSION, BLUEZ, DEVICE);
    broker
}

#[test]
fn interactive_prompts_publish_typed_state_and_accept_responses() {
    let broker = broker();

    let pin_broker = broker.clone();
    let pin = smol::spawn(async move {
        pin_broker
            .request_pin_code(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });
    smol::block_on(async {
        while !broker.prompt().active {
            async_io::Timer::after(Duration::from_millis(1)).await;
        }
        let prompt = broker.prompt();
        assert_eq!(prompt.kind, AgentPromptKind::PinCodeInput);
        assert_eq!(prompt.pairing_epoch, EPOCH);
        assert_eq!(prompt.device_path, DEVICE);
        assert_eq!(prompt.request_id, 1);
        assert_eq!(
            broker.submit_text(prompt.request_id, "1234"),
            AgentSubmitResult::Accepted
        );
        assert_eq!(pin.await, Ok("1234".to_owned()));
    });

    let passkey_broker = broker.clone();
    let passkey = smol::spawn(async move {
        passkey_broker
            .request_passkey(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });
    smol::block_on(async {
        while broker.prompt().kind != AgentPromptKind::PasskeyInput {
            async_io::Timer::after(Duration::from_millis(1)).await;
        }
        let request_id = broker.prompt().request_id;
        assert_eq!(
            broker.submit_text(request_id, "004201"),
            AgentSubmitResult::Accepted
        );
        assert_eq!(passkey.await, Ok(4201));
    });

    for (kind, result) in [
        (AgentPromptKind::PasskeyConfirmation, true),
        (AgentPromptKind::Authorization, true),
        (AgentPromptKind::ServiceAuthorization, false),
    ] {
        let request_broker = broker.clone();
        let request = smol::spawn(async move {
            match kind {
                AgentPromptKind::PasskeyConfirmation => request_broker
                    .request_confirmation(SENDER, SESSION, BLUEZ, DEVICE, 4201)
                    .await
                    .map(|()| true),
                AgentPromptKind::Authorization => request_broker
                    .request_authorization(SENDER, SESSION, BLUEZ, DEVICE)
                    .await
                    .map(|()| true),
                AgentPromptKind::ServiceAuthorization => request_broker
                    .authorize_service(
                        SENDER,
                        SESSION,
                        BLUEZ,
                        DEVICE,
                        "0000110b-0000-1000-8000-00805f9b34fb",
                    )
                    .await
                    .map(|()| true),
                _ => unreachable!(),
            }
        });
        smol::block_on(async {
            while broker.prompt().kind != kind {
                async_io::Timer::after(Duration::from_millis(1)).await;
            }
            let prompt = broker.prompt();
            if kind == AgentPromptKind::ServiceAuthorization {
                assert_eq!(
                    prompt.service_uuid.as_deref(),
                    Some("0000110b-0000-1000-8000-00805f9b34fb")
                );
            }
            assert_eq!(
                broker.confirm(prompt.request_id, result),
                AgentSubmitResult::Accepted
            );
            let response = request.await;
            if result {
                assert_eq!(response, Ok(true));
            } else {
                assert_eq!(response, Err(AgentError::Rejected));
            }
        });
    }
}

#[test]
fn display_prompts_are_immediate_and_repeated_passkeys_coalesce() {
    let broker = broker();
    broker
        .display_pin_code(SENDER, SESSION, BLUEZ, DEVICE, "1234")
        .expect("display PIN");
    let pin = broker.prompt();
    assert_eq!(pin.kind, AgentPromptKind::DisplayPinCode);
    assert_eq!(pin.display_pin.as_deref(), Some("1234"));

    broker
        .display_passkey(SENDER, SESSION, BLUEZ, DEVICE, 42, 0)
        .expect("display passkey");
    let first = broker.prompt();
    assert_eq!(first.kind, AgentPromptKind::DisplayPasskey);
    assert_eq!(first.passkey, Some(42));
    assert_eq!(first.entered, Some(0));

    broker
        .display_passkey(SENDER, SESSION, BLUEZ, DEVICE, 420042, 3)
        .expect("update displayed passkey");
    let second = broker.prompt();
    assert_eq!(second.request_id, first.request_id);
    assert_eq!(second.passkey, Some(420042));
    assert_eq!(second.entered, Some(3));
}

#[test]
fn validation_stale_ids_and_second_interactive_request_are_deterministic() {
    let broker = broker();
    let first_broker = broker.clone();
    let first = smol::spawn(async move {
        first_broker
            .request_pin_code(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });

    smol::block_on(async {
        while !broker.prompt().active {
            async_io::Timer::after(Duration::from_millis(1)).await;
        }
        let request_id = broker.prompt().request_id;
        assert_eq!(
            broker.submit_text(request_id + 1, "1234"),
            AgentSubmitResult::Ignored
        );
        assert_eq!(
            broker.submit_text(request_id, ""),
            AgentSubmitResult::Invalid
        );
        assert_eq!(
            broker.submit_text(request_id, "12345678901234567"),
            AgentSubmitResult::Invalid
        );

        let second = broker
            .request_authorization(SENDER, SESSION, BLUEZ, DEVICE)
            .await;
        assert_eq!(second, Err(AgentError::Rejected));

        assert_eq!(
            broker.submit_text(request_id, "1234"),
            AgentSubmitResult::Accepted
        );
        assert_eq!(first.await, Ok("1234".to_owned()));
        assert_eq!(broker.reject(request_id), AgentSubmitResult::Ignored);
    });
}

#[test]
fn lifecycle_cancellation_timeout_and_owner_authentication_clear_prompts() {
    let broker = broker();
    assert_eq!(broker.authorize_call(SENDER, SESSION, BLUEZ), Ok(()));
    assert_eq!(
        broker.authorize_call(":1.99", SESSION, BLUEZ),
        Err(AgentError::Rejected)
    );
    assert_eq!(
        broker.authorize_call(SENDER, SESSION, BLUEZ + 1),
        Err(AgentError::Rejected)
    );

    let cancel_broker = broker.clone();
    let canceled = smol::spawn(async move {
        cancel_broker
            .request_pin_code(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });
    smol::block_on(async {
        while !broker.prompt().active {
            async_io::Timer::after(Duration::from_millis(1)).await;
        }
        let request_id = broker.prompt().request_id;
        broker.cancel_for_lifecycle();
        assert_eq!(canceled.await, Err(AgentError::Canceled));
        assert!(!broker.prompt().active);
        assert_eq!(
            broker.submit_text(request_id, "1234"),
            AgentSubmitResult::Ignored
        );
    });

    broker.set_pairing_context(EPOCH + 1, SESSION, BLUEZ, DEVICE);
    let timeout_broker = broker.clone();
    let timed_out = smol::spawn(async move {
        timeout_broker
            .request_authorization(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });
    assert_eq!(smol::block_on(timed_out), Err(AgentError::Canceled));
    assert!(!broker.prompt().active);
}

#[test]
fn stale_owner_and_release_cancel_without_publishing_unauthorized_prompts() {
    let broker = broker();
    assert_eq!(
        broker.display_pin_code(":1.99", SESSION, BLUEZ, DEVICE, "1234"),
        Err(AgentError::Rejected)
    );
    assert!(!broker.prompt().active);

    let request_broker = broker.clone();
    let request = smol::spawn(async move {
        request_broker
            .request_authorization(SENDER, SESSION, BLUEZ, DEVICE)
            .await
    });
    smol::block_on(async {
        while !broker.prompt().active {
            async_io::Timer::after(Duration::from_millis(1)).await;
        }
        broker.set_authority(SESSION, BLUEZ + 1, ":1.43");
        assert_eq!(request.await, Err(AgentError::Canceled));
        assert!(!broker.prompt().active);
    });
}

#[test]
fn secret_values_are_not_present_in_debug_diagnostics() {
    let broker = broker();
    broker
        .display_passkey(SENDER, SESSION, BLUEZ, DEVICE, 123456, 0)
        .expect("display passkey");
    let debug = format!("{:?}", broker.prompt());
    assert!(!debug.contains("123456"));
    assert!(!debug.contains("1234"));
}
