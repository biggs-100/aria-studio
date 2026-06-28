// ARIA DAW — Database Migration System

pub struct Migration {
    pub version: u32,
    pub description: &'static str,
    pub sql: &'static str,
}

/// Ordered list of database migrations.
pub const MIGRATIONS: &[Migration] = &[
    Migration {
        version: 1,
        description: "Initial schema",
        sql: super::schema::CREATE_SCHEMA_SQL,
    },
];
