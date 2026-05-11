use quick_xml::Reader;
use quick_xml::events::Event;
use serde::{Deserialize, Serialize};
use std::path::Path;

use crate::assetmap::strip_urn_uuid;

/// A single asset in the PKL.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PklAsset {
    pub id: String,
    #[serde(rename = "type")]
    pub asset_type: String,
    pub original_filename: String,
    pub hash: String,
    pub hash_algorithm: String,
    pub size: i64,
}

/// Parsed Packing List (PKL).
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Pkl {
    pub id: String,
    pub creator: String,
    pub issue_date: String,
    pub assets: Vec<PklAsset>,
}

impl Pkl {
    /// Parse a PKL XML file.
    pub fn parse(file: &Path) -> Option<Self> {
        let xml = std::fs::read_to_string(file).ok()?;
        let mut reader = Reader::from_str(&xml);

        // Quick check: is this a PackingList?
        let mut found_root = false;
        {
            let mut peek_reader = Reader::from_str(&xml);
            loop {
                match peek_reader.read_event() {
                    Ok(Event::Start(e)) => {
                        let name = local_name(&e);
                        if name == "PackingList" {
                            found_root = true;
                        }
                        break;
                    }
                    Ok(Event::Eof) => break,
                    Err(_) => break,
                    _ => {}
                }
            }
        }
        if !found_root {
            return None;
        }

        let mut pkl = Pkl::default();
        let mut in_asset = false;
        let mut current_asset = PklAsset::default();
        let mut current_tag = String::new();

        loop {
            match reader.read_event() {
                Ok(Event::Start(e)) => {
                    let name = local_name(&e);
                    match name.as_str() {
                        "Asset" => {
                            in_asset = true;
                            current_asset = PklAsset::default();
                        }
                        _ => current_tag = name,
                    }
                }
                Ok(Event::End(e)) => {
                    let name = local_name_end(&e);
                    if name == "Asset" && in_asset {
                        if !current_asset.id.is_empty() {
                            pkl.assets.push(std::mem::take(&mut current_asset));
                        }
                        in_asset = false;
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
                            "Id" => current_asset.id = strip_urn_uuid(&text),
                            "Type" => current_asset.asset_type = text,
                            "OriginalFileName" => current_asset.original_filename = text,
                            "Hash" => current_asset.hash = text,
                            "HashAlgorithm" => current_asset.hash_algorithm = text,
                            "Size" => current_asset.size = text.parse().unwrap_or(0),
                            _ => {}
                        }
                    } else {
                        match current_tag.as_str() {
                            "Id" => pkl.id = strip_urn_uuid(&text),
                            "Creator" => pkl.creator = text,
                            "IssueDate" => pkl.issue_date = text,
                            _ => {}
                        }
                    }
                }
                Ok(Event::Eof) => break,
                Err(_) => return None,
                _ => {}
            }
        }

        Some(pkl)
    }
}

fn local_name(e: &quick_xml::events::BytesStart) -> String {
    String::from_utf8_lossy(e.local_name().as_ref()).to_string()
}

fn local_name_end(e: &quick_xml::events::BytesEnd) -> String {
    String::from_utf8_lossy(e.local_name().as_ref()).to_string()
}
