//! Country and time-zone catalogue logic for the Language & Region page.
//!
//! Nothing here touches the network. `nearest_zone` turns coordinates into a
//! zone using only the system's tzdata `zone.tab`, and `parse_ip_lookup` reads a
//! response the page's explicit IP lookup already obtained.
use core::ffi::c_void;
use core::slice;

#[derive(Clone, Debug, PartialEq)]
pub struct Zone<'a> {
    pub country: &'a str,
    pub id: &'a str,
    pub comment: &'a str,
    pub latitude: f64,
    pub longitude: f64,
}

/// One ISO 6709 component: sign, `degree_digits` of degrees, minutes, optional seconds.
fn parse_angle(text: &str, degree_digits: usize) -> Option<f64> {
    let sign = match text.as_bytes().first()? {
        b'+' => 1.0,
        b'-' => -1.0,
        _ => return None,
    };
    let digits = &text[1..];
    if !digits.bytes().all(|byte| byte.is_ascii_digit())
        || (digits.len() != degree_digits + 2 && digits.len() != degree_digits + 4)
    {
        return None;
    }
    let degrees: f64 = digits[..degree_digits].parse().ok()?;
    let minutes: f64 = digits[degree_digits..degree_digits + 2].parse().ok()?;
    let seconds: f64 = match digits.get(degree_digits + 2..) {
        Some("") | None => 0.0,
        Some(value) => value.parse().ok()?,
    };
    Some(sign * (degrees + minutes / 60.0 + seconds / 3600.0))
}

/// `±DDMM±DDDMM` or `±DDMMSS±DDDMMSS`, as used by tzdata.
pub fn parse_coordinates(text: &str) -> Option<(f64, f64)> {
    if !text.is_ascii() {
        return None;
    }
    let split = text.get(1..)?.find(['+', '-'])? + 1;
    let latitude = parse_angle(&text[..split], 2)?;
    let longitude = parse_angle(&text[split..], 3)?;
    valid_coordinates(latitude, longitude).then_some((latitude, longitude))
}

pub fn valid_coordinates(latitude: f64, longitude: f64) -> bool {
    latitude.is_finite()
        && longitude.is_finite()
        && latitude.abs() <= 90.0
        && longitude.abs() <= 180.0
}

pub fn valid_country_code(code: &str) -> bool {
    code.len() == 2 && code.bytes().all(|byte| byte.is_ascii_uppercase())
}

pub fn valid_zone_id(id: &str) -> bool {
    !id.is_empty()
        && id.len() <= 64
        && !id.starts_with('/')
        && !id.contains("..")
        && id
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'/' | b'_' | b'+' | b'-'))
}

/// Reads tzdata's `zone.tab`. Malformed lines are skipped, not fatal: a partial
/// catalogue beats none, and the file is distribution data outside our control.
pub fn parse_zone_tab(text: &str) -> Vec<Zone<'_>> {
    text.lines()
        .filter(|line| !line.starts_with('#'))
        .filter_map(|line| {
            let mut fields = line.split('\t');
            let country = fields.next()?;
            let (latitude, longitude) = parse_coordinates(fields.next()?)?;
            let id = fields.next()?;
            (valid_country_code(country) && valid_zone_id(id)).then_some(Zone {
                country,
                id,
                comment: fields.next().unwrap_or(""),
                latitude,
                longitude,
            })
        })
        .collect()
}

/// Great-circle distance in kilometres.
pub fn distance_km(from: (f64, f64), to: (f64, f64)) -> f64 {
    const EARTH_RADIUS_KM: f64 = 6371.0;
    let (lat1, lat2) = (from.0.to_radians(), to.0.to_radians());
    let half_lat = (lat2 - lat1) / 2.0;
    let half_lon = (to.1 - from.1).to_radians() / 2.0;
    let a = half_lat.sin().powi(2) + lat1.cos() * lat2.cos() * half_lon.sin().powi(2);
    2.0 * EARTH_RADIUS_KM * a.sqrt().min(1.0).asin()
}

/// The zone whose reference city is closest. This approximates the real border
/// (tzdata has no polygons), which is why the page asks before applying it.
pub fn nearest_zone<'a, 'z>(
    zones: &'z [Zone<'a>],
    latitude: f64,
    longitude: f64,
) -> Option<&'z Zone<'a>> {
    if !valid_coordinates(latitude, longitude) {
        return None;
    }
    zones.iter().min_by(|left, right| {
        let distance = |zone: &Zone| distance_km((latitude, longitude), (zone.latitude, zone.longitude));
        distance(left).total_cmp(&distance(right))
    })
}

fn word_prefix(field: &str, needle: &str) -> bool {
    field
        .split(|c: char| !c.is_alphanumeric() && c != '+' && c != '-')
        .any(|word| word.starts_with(needle))
}

/// Lower is better; `None` excludes the entry. Fields are tab-separated with the
/// display name first. Callers fold accents and case before calling.
fn match_score(query: &str, key: &str) -> Option<u8> {
    let mut fields = key.split('\t');
    let primary = fields.next().unwrap_or("");
    if key.split('\t').any(|field| field == query) {
        return Some(0);
    }
    if primary.starts_with(query) {
        return Some(1);
    }
    if key.split('\t').any(|field| field.starts_with(query)) {
        return Some(2);
    }
    if key.split('\t').any(|field| word_prefix(field, query)) {
        return Some(3);
    }
    // One or two letters as a bare substring would match most of the list.
    if query.chars().count() >= 3 && key.contains(query) {
        return Some(4);
    }
    let mut tokens = query.split_whitespace().peekable();
    tokens.peek()?;
    tokens
        .all(|token| {
            key.split('\t').any(|field| word_prefix(field, token))
                || (token.chars().count() >= 3 && key.contains(token))
        })
        .then_some(5)
}

/// Indices of matching keys, best first; ties keep the catalogue order.
pub fn rank(query: &str, keys: &[&str]) -> Vec<usize> {
    let query = query.trim();
    if query.is_empty() {
        return (0..keys.len()).collect();
    }
    let mut scored: Vec<(u8, usize)> = keys
        .iter()
        .enumerate()
        .filter_map(|(index, key)| Some((match_score(query, key)?, index)))
        .collect();
    scored.sort();
    scored.into_iter().map(|(_, index)| index).collect()
}

#[derive(Debug, PartialEq)]
pub enum IpLookup {
    Found { country: String, zone: String },
    Refused,
    Invalid,
}

/// Reads an `ipapi.co/json/` body. Only the country and the zone are kept; the
/// coordinates, the address and the network owner in the reply are never read.
pub fn parse_ip_lookup(body: &[u8]) -> IpLookup {
    #[derive(serde::Deserialize)]
    struct Reply {
        #[serde(default)]
        error: bool,
        country_code: Option<String>,
        timezone: Option<String>,
    }
    let Ok(reply) = serde_json::from_slice::<Reply>(body) else {
        return IpLookup::Invalid;
    };
    if reply.error {
        return IpLookup::Refused;
    }
    match (reply.country_code, reply.timezone) {
        (Some(country), Some(zone)) if valid_country_code(&country) && valid_zone_id(&zone) => {
            IpLookup::Found { country, zone }
        }
        _ => IpLookup::Invalid,
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Text {
    data: *const u8,
    size: usize,
}

impl Text {
    fn view(value: &str) -> Self {
        Self { data: value.as_ptr(), size: value.len() }
    }

    /// # Safety
    /// `data` must address `size` readable bytes, or `size` must be zero.
    unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.size == 0 || self.data.is_null() {
            &[]
        } else {
            unsafe { slice::from_raw_parts(self.data, self.size) }
        }
    }

    /// # Safety
    /// As for [`Text::bytes`].
    unsafe fn text<'a>(self) -> &'a str {
        core::str::from_utf8(unsafe { self.bytes() }).unwrap_or("")
    }
}

#[repr(C)]
pub struct ZoneView {
    country: Text,
    id: Text,
    comment: Text,
    latitude: f64,
    longitude: f64,
}

impl ZoneView {
    fn new(zone: &Zone) -> Self {
        Self {
            country: Text::view(zone.country),
            id: Text::view(zone.id),
            comment: Text::view(zone.comment),
            latitude: zone.latitude,
            longitude: zone.longitude,
        }
    }
}

type ZoneCallback = extern "C" fn(*mut c_void, *const ZoneView);
type IpCallback = extern "C" fn(*mut c_void, Text, Text);

/// # Safety
/// `table` must be readable; `context` is passed to `emit` untouched. Views are call-scoped.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_region_parse_zones(
    table: Text,
    emit: ZoneCallback,
    context: *mut c_void,
) -> usize {
    let zones = parse_zone_tab(unsafe { table.text() });
    for zone in &zones {
        emit(context, &ZoneView::new(zone));
    }
    zones.len()
}

/// # Safety
/// As for [`astrea_region_parse_zones`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_region_nearest_zone(
    table: Text,
    latitude: f64,
    longitude: f64,
    emit: ZoneCallback,
    context: *mut c_void,
) -> bool {
    let zones = parse_zone_tab(unsafe { table.text() });
    match nearest_zone(&zones, latitude, longitude) {
        Some(zone) => {
            emit(context, &ZoneView::new(zone));
            true
        }
        None => false,
    }
}

/// Writes up to `count` indices to `out` and returns how many matched.
///
/// # Safety
/// `keys` must address `count` readable views and `out` `count` writable indices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_region_rank(
    query: Text,
    keys: *const Text,
    count: usize,
    out: *mut usize,
) -> usize {
    if count == 0 || keys.is_null() || out.is_null() {
        return 0;
    }
    let keys: Vec<&str> = unsafe { slice::from_raw_parts(keys, count) }
        .iter()
        .map(|key| unsafe { key.text() })
        .collect();
    let ranked = rank(unsafe { query.text() }, &keys);
    let out = unsafe { slice::from_raw_parts_mut(out, count) };
    out[..ranked.len()].copy_from_slice(&ranked);
    ranked.len()
}

/// Returns 0 for an unusable body, 1 for a location and 2 when the service refused.
///
/// # Safety
/// As for [`astrea_region_parse_zones`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_region_parse_ip_lookup(
    body: Text,
    emit: IpCallback,
    context: *mut c_void,
) -> u8 {
    match parse_ip_lookup(unsafe { body.bytes() }) {
        IpLookup::Invalid => 0,
        IpLookup::Refused => 2,
        IpLookup::Found { country, zone } => {
            emit(context, Text::view(&country), Text::view(&zone));
            1
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const TABLE: &str = "# comment line\n\
        BR\t-2332-04637\tAmerica/Sao_Paulo\tBrazil (southeast: GO, DF, MG, ES, RJ, SP, PR, SC, RS)\n\
        BR\t-0308-06001\tAmerica/Manaus\tAmazonas (east)\n\
        PT\t+3843-00908\tEurope/Lisbon\tPortugal (mainland)\n\
        JP\t+353916+1394441\tAsia/Tokyo\n\
        NZ\t-3652+17446\tPacific/Auckland\tmost of New Zealand\n\
        US\t+404251-0740023\tAmerica/New_York\tEastern (most areas)\n\
        broken line\n\
        xx\t-2332-04637\tBad/Country\n\
        BR\tnot-coords\tBad/Coords\n";

    #[test]
    fn coordinates_read_both_tzdata_precisions() {
        let (lat, lon) = parse_coordinates("-2332-04637").unwrap();
        assert!((lat - -23.5333).abs() < 0.001, "{lat}");
        assert!((lon - -46.6167).abs() < 0.001, "{lon}");
        let (lat, lon) = parse_coordinates("+353916+1394441").unwrap();
        assert!((lat - 35.6544).abs() < 0.001, "{lat}");
        assert!((lon - 139.7447).abs() < 0.001, "{lon}");
    }

    #[test]
    fn coordinates_reject_garbage_and_out_of_range() {
        for text in ["", "+", "-2332", "2332-04637", "+9900+00000", "+0000+18100", "+00a0+00000", "é+0000"] {
            assert_eq!(parse_coordinates(text), None, "{text}");
        }
    }

    #[test]
    fn zone_table_skips_comments_and_malformed_lines() {
        let zones = parse_zone_tab(TABLE);
        assert_eq!(zones.len(), 6);
        assert_eq!(zones[0].country, "BR");
        assert_eq!(zones[0].id, "America/Sao_Paulo");
        assert_eq!(zones[3].comment, "");
    }

    #[test]
    fn system_zone_table_covers_many_countries_when_installed() {
        let Ok(text) = std::fs::read_to_string("/usr/share/zoneinfo/zone.tab") else {
            return;
        };
        let zones = parse_zone_tab(&text);
        assert!(zones.len() > 300, "{}", zones.len());
        assert!(zones.iter().any(|zone| zone.id == "America/Sao_Paulo" && zone.country == "BR"));
    }

    #[test]
    fn nearest_zone_finds_the_closest_reference_city() {
        let zones = parse_zone_tab(TABLE);
        // Campinas, Porto, Osaka, and a point across the antimeridian from Auckland.
        assert_eq!(nearest_zone(&zones, -22.9, -47.06).unwrap().id, "America/Sao_Paulo");
        assert_eq!(nearest_zone(&zones, 41.15, -8.61).unwrap().id, "Europe/Lisbon");
        assert_eq!(nearest_zone(&zones, 34.69, 135.5).unwrap().id, "Asia/Tokyo");
        assert_eq!(nearest_zone(&zones, -36.0, -179.0).unwrap().id, "Pacific/Auckland");
    }

    #[test]
    fn nearest_zone_refuses_invalid_input() {
        let zones = parse_zone_tab(TABLE);
        assert!(nearest_zone(&zones, 91.0, 0.0).is_none());
        assert!(nearest_zone(&zones, f64::NAN, 0.0).is_none());
        assert!(nearest_zone(&[], 0.0, 0.0).is_none());
    }

    #[test]
    fn distance_matches_a_known_route() {
        // Sao Paulo to Lisbon is about 7,950 km.
        let distance = distance_km((-23.55, -46.63), (38.72, -9.14));
        assert!((distance - 7950.0).abs() < 100.0, "{distance}");
    }

    #[test]
    fn rank_prefers_exact_then_prefix_then_substring() {
        let keys = ["gibraltar\tgi", "brasil\tbrazil\tbr\tbra", "bermudas\tbm", "libreria\tzz"];
        assert_eq!(rank("br", &keys), [1]);
        assert_eq!(rank("bra", &keys), [1, 0]);
        assert_eq!(rank("bre", &keys), [3]);
    }

    #[test]
    fn rank_finds_zone_words_in_any_order() {
        let keys = ["sao paulo\tamerica sao paulo\tbrasil\tutc-3", "lisboa\teurope lisbon\tportugal\tutc+0"];
        assert_eq!(rank("paulo", &keys), [0]);
        assert_eq!(rank("brasil sao", &keys), [0]);
        assert_eq!(rank("utc-3", &keys), [0]);
        assert_eq!(rank("europe", &keys), [1]);
    }

    #[test]
    fn rank_with_empty_query_keeps_everything_in_order() {
        assert_eq!(rank("   ", &["b", "a"]), [0, 1]);
        assert!(rank("zz", &["brasil", "chile"]).is_empty());
    }

    #[test]
    fn ip_lookup_keeps_only_country_and_zone() {
        let body = br#"{"ip":"203.0.113.5","city":"X","country_code":"BR","timezone":"America/Sao_Paulo","latitude":-23.5}"#;
        assert_eq!(
            parse_ip_lookup(body),
            IpLookup::Found { country: "BR".into(), zone: "America/Sao_Paulo".into() }
        );
    }

    #[test]
    fn ip_lookup_reports_refusal_and_rejects_bad_values() {
        assert_eq!(parse_ip_lookup(br#"{"error":true,"reason":"RateLimited"}"#), IpLookup::Refused);
        assert_eq!(parse_ip_lookup(b"<html>"), IpLookup::Invalid);
        assert_eq!(parse_ip_lookup(br#"{"country_code":"BR"}"#), IpLookup::Invalid);
        assert_eq!(
            parse_ip_lookup(br#"{"country_code":"br!","timezone":"America/Sao_Paulo"}"#),
            IpLookup::Invalid
        );
        assert_eq!(
            parse_ip_lookup(br#"{"country_code":"BR","timezone":"../../etc/passwd"}"#),
            IpLookup::Invalid
        );
    }
}
