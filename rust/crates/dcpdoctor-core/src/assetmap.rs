use quick_xml::Reader;
use quick_xml::events::Event;
use serde::{Deserialize, Serialize};
use std::path::Path;

/// A single asset entry in the ASSETMAP.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Asset {
    pub id: String,
    pub path: String,
}

/// Parsed ASSETMAP.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AssetMap {
    pub id: String,
    pub creator: String,
    pub issue_date: String,
    pub assets: Vec<Asset>,
}

impl AssetMap {
    /// Parse an ASSETMAP or ASSETMAP.xml file.
    pub fn parse(file: &Path) -> Option<Self> {
        let xml = std::fs::read_to_string(file).ok()?;
        let mut reader = Reader::from_str(&xml);

        let mut am = AssetMap::default();
        let mut in_asset = false;
        let mut in_chunk = false;
        let mut current_asset = Asset::default();
        let mut current_tag = String::new();

        loop {
            match reader.read_event() {
                Ok(Event::Start(e)) => {
                    let name = local_name(&e);
                    match name.as_str() {
                        "AssetMap" => {}
                        "Asset" => {
                            in_asset = true;
                            current_asset = Asset::default();
                        }
                        "Chunk" => in_chunk = true,
                        _ => current_tag = name,
                    }
                }
                Ok(Event::End(e)) => {
                    let name = local_name_end(&e);
                    match name.as_str() {
                        "Asset" => {
                            if in_asset && !current_asset.id.is_empty() {
                                am.assets.push(std::mem::take(&mut current_asset));
                            }
                            in_asset = false;
                        }
                        "Chunk" => in_chunk = false,
                        _ => {}
                    }
                    current_tag.clear();
                }
                Ok(Event::Text(e)) => {
                    let text = e.unescape().ok().map(|s| s.to_string()).unwrap_or_default();
                    if text.is_empty() {
                        continue;
                    }
                    if in_asset {
                        match current_tag.as_str() {
                            "Id" if !in_chunk => current_asset.id = strip_urn_uuid(&text),
                            "Path" if in_chunk => current_asset.path = text,
                            _ => {}
                        }
                    } else {
                        match current_tag.as_str() {
                            "Id" => am.id = strip_urn_uuid(&text),
                            "Creator" => am.creator = text,
                            "IssueDate" => am.issue_date = text,
                            _ => {}
                        }
                    }
                }
                Ok(Event::Eof) => break,
                Err(_) => return None,
                _ => {}
            }
        }

        Some(am)
    }
}

/// Strip "urn:uuid:" prefix from UUID strings.
pub fn strip_urn_uuid(s: &str) -> String {
    s.strip_prefix("urn:uuid:").unwrap_or(s).to_string()
}

/// Get local name from a start element (strip namespace prefix).
fn local_name(e: &quick_xml::events::BytesStart) -> String {
    let name = e.local_name();
    String::from_utf8_lossy(name.as_ref()).to_string()
}

fn local_name_end(e: &quick_xml::events::BytesEnd) -> String {
    let name = e.local_name();
    String::from_utf8_lossy(name.as_ref()).to_string()
}
