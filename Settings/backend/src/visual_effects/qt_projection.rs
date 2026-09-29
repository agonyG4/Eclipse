use super::material_program::MaterialProgramState;
use crate::typhon::material_program::{
    MaterialProgramParameterDescriptor, MaterialProgramParameterRange,
    MaterialProgramParameterType, MaterialProgramParameterValue,
};
use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QVariant};

pub fn catalog(state: &MaterialProgramState) -> QList<QVariant> {
    let mut catalog = QList::default();
    if let Some(snapshot) = state.catalog() {
        for program in &snapshot.programs {
            let mut entry = QMap::default();
            insert_string(&mut entry, "name", &program.name);
            insert_string(&mut entry, "origin", origin_name(program.origin));
            insert_variant(
                &mut entry,
                "parameterCount",
                QVariant::from(&i32::from(program.parameter_count)),
            );
            catalog.append(QVariant::from(&entry));
        }
    }
    catalog
}

pub fn parameters(state: &MaterialProgramState) -> QList<QVariant> {
    let mut parameters = QList::default();
    if let Some(description) = state.description() {
        for descriptor in &description.parameters {
            parameters.append(QVariant::from(&parameter(state, descriptor)));
        }
    }
    parameters
}

fn parameter(
    state: &MaterialProgramState,
    descriptor: &MaterialProgramParameterDescriptor,
) -> QMap<QMapPair_QString_QVariant> {
    let mut parameter = QMap::default();
    insert_string(&mut parameter, "name", &descriptor.name);
    insert_string(
        &mut parameter,
        "parameterType",
        parameter_type_name(descriptor.parameter_type),
    );
    insert_variant(
        &mut parameter,
        "range",
        range_variant(descriptor.range.as_ref()),
    );
    insert_variant(&mut parameter, "default", value_variant(descriptor.default));
    let value = state
        .displayed_parameter_value(&descriptor.name)
        .unwrap_or(descriptor.effective);
    insert_variant(&mut parameter, "value", value_variant(value));
    insert_bool(
        &mut parameter,
        "overridden",
        state.displayed_parameter_overridden(&descriptor.name),
    );
    parameter
}

fn range_variant(range: Option<&MaterialProgramParameterRange>) -> QVariant {
    let Some(range) = range else {
        return QVariant::default();
    };
    let mut map = QMap::default();
    match range {
        MaterialProgramParameterRange::Float { min, max } => {
            insert_string(&mut map, "type", "float");
            insert_variant(&mut map, "min", QVariant::from(min));
            insert_variant(&mut map, "max", QVariant::from(max));
        }
        MaterialProgramParameterRange::FloatComponents {
            components,
            min,
            max,
        } => {
            insert_string(&mut map, "type", "float_components");
            insert_variant(
                &mut map,
                "components",
                QVariant::from(&i32::from(*components)),
            );
            insert_variant(&mut map, "min", number_list(min));
            insert_variant(&mut map, "max", number_list(max));
        }
        MaterialProgramParameterRange::Int { min, max } => {
            insert_string(&mut map, "type", "int");
            insert_variant(&mut map, "min", QVariant::from(min));
            insert_variant(&mut map, "max", QVariant::from(max));
        }
    }
    QVariant::from(&map)
}

fn value_variant(value: MaterialProgramParameterValue) -> QVariant {
    match value {
        MaterialProgramParameterValue::Float(value) => QVariant::from(&value),
        MaterialProgramParameterValue::Vec2(values) => number_list(&values),
        MaterialProgramParameterValue::Vec3(values) => number_list(&values),
        MaterialProgramParameterValue::Vec4(values) => number_list(&values),
        MaterialProgramParameterValue::Int(value) => QVariant::from(&value),
    }
}

fn number_list(values: &[f64]) -> QVariant {
    let mut list = QList::default();
    for value in values {
        list.append(QVariant::from(value));
    }
    QVariant::from(&list)
}

fn parameter_type_name(parameter_type: MaterialProgramParameterType) -> &'static str {
    match parameter_type {
        MaterialProgramParameterType::Float => "float",
        MaterialProgramParameterType::Vec2 => "vec2",
        MaterialProgramParameterType::Vec3 => "vec3",
        MaterialProgramParameterType::Vec4 => "vec4",
        MaterialProgramParameterType::Int => "int",
    }
}

fn origin_name(origin: crate::typhon::material_program::MaterialProgramOrigin) -> &'static str {
    match origin {
        crate::typhon::material_program::MaterialProgramOrigin::System => "system",
        crate::typhon::material_program::MaterialProgramOrigin::TrustedLocal => "trusted_local",
    }
}

fn insert_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: &str) {
    map.insert(QString::from(key), QVariant::from(&QString::from(value)));
}

fn insert_bool(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: bool) {
    map.insert(QString::from(key), QVariant::from(&value));
}

fn insert_variant(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: QVariant) {
    map.insert(QString::from(key), value);
}
