use std::ffi::{CStr, CString};
use std::os::raw::c_char;

pub unsafe fn c_str_to_str<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() {
        return "";
    }
    unsafe { CStr::from_ptr(ptr).to_str().unwrap_or("") }
}

pub unsafe fn set_error(error_out: *mut *mut c_char, msg: &str) {
    if error_out.is_null() {
        return;
    }
    if let Ok(c_str) = CString::new(msg) {
        unsafe { *error_out = c_str.into_raw() };
    }
}

pub unsafe fn free_c_string(value: *mut c_char) {
    if !value.is_null() {
        drop(unsafe { CString::from_raw(value) });
    }
}
