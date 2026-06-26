use super::conversion::{c_str_to_str, free_c_string, set_error};
use crate::{AstreaSpotlightBackend, config};
use std::ffi::CString;
use std::os::raw::c_char;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_create(
    astrea_root: *const c_char,
    locale: *const c_char,
    error_out: *mut *mut c_char,
) -> *mut AstreaSpotlightBackend {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let root = unsafe { c_str_to_str(astrea_root) };
        let loc = unsafe { c_str_to_str(locale) };
        match AstreaSpotlightBackend::new(root, loc) {
            Ok(backend) => Box::into_raw(Box::new(backend)),
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                std::ptr::null_mut()
            }
        }
    }));

    match result {
        Ok(ptr) => ptr,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in create");
            }
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_destroy(backend: *mut AstreaSpotlightBackend) {
    let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if !backend.is_null() {
            drop(unsafe { Box::from_raw(backend) });
        }
    }));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_reload(
    backend: *mut AstreaSpotlightBackend,
    error_out: *mut *mut c_char,
) -> i32 {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if backend.is_null() {
            unsafe {
                set_error(error_out, "null backend");
            }
            return -1;
        }
        match unsafe { (*backend).reload() } {
            Ok(()) => 0,
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                -1
            }
        }
    }));

    match result {
        Ok(v) => v,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in reload");
            }
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_search_json(
    backend: *mut AstreaSpotlightBackend,
    query: *const c_char,
    limit: usize,
    error_out: *mut *mut c_char,
) -> *mut c_char {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if backend.is_null() {
            unsafe {
                set_error(error_out, "null backend");
            }
            return std::ptr::null_mut();
        }
        let q = unsafe { c_str_to_str(query) };
        match unsafe { (*backend).search_json(q, limit) } {
            Ok(json) => CString::new(json).unwrap_or_default().into_raw(),
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                std::ptr::null_mut()
            }
        }
    }));

    match result {
        Ok(ptr) => ptr,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in search_json");
            }
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_record_launch(
    backend: *mut AstreaSpotlightBackend,
    desktop_id: *const c_char,
    error_out: *mut *mut c_char,
) -> i32 {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if backend.is_null() {
            unsafe {
                set_error(error_out, "null backend");
            }
            return -1;
        }
        let id = unsafe { c_str_to_str(desktop_id) };
        match unsafe { (*backend).record_launch(id) } {
            Ok(()) => 0,
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                -1
            }
        }
    }));

    match result {
        Ok(v) => v,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in record_launch");
            }
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_record_activation(
    backend: *mut AstreaSpotlightBackend,
    desktop_id: *const c_char,
    error_out: *mut *mut c_char,
) -> i32 {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if backend.is_null() {
            unsafe {
                set_error(error_out, "null backend");
            }
            return -1;
        }
        let id = unsafe { c_str_to_str(desktop_id) };
        match unsafe { (*backend).record_activation(id) } {
            Ok(()) => 0,
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                -1
            }
        }
    }));

    match result {
        Ok(v) => v,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in record_activation");
            }
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_ensure_config(
    error_out: *mut *mut c_char,
) -> i32 {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        match config::store::write_default_config() {
            Ok(()) => 0,
            Err(e) => {
                unsafe {
                    set_error(error_out, &e);
                }
                -1
            }
        }
    }));
    match result {
        Ok(v) => v,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in ensure_config");
            }
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_free_string(value: *mut c_char) {
    unsafe { free_c_string(value) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_spotlight_backend_watched_dirs(
    backend: *mut AstreaSpotlightBackend,
    error_out: *mut *mut c_char,
) -> *mut c_char {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if backend.is_null() {
            unsafe {
                set_error(error_out, "null backend");
            }
            return std::ptr::null_mut();
        }
        let dirs = unsafe { (*backend).index.watcher_dirs() };
        let json = serde_json::to_string(dirs).unwrap_or_default();
        CString::new(json).unwrap_or_default().into_raw()
    }));
    match result {
        Ok(ptr) => ptr,
        Err(_) => {
            unsafe {
                set_error(error_out, "panic in watched_dirs");
            }
            std::ptr::null_mut()
        }
    }
}
