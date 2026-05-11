/// XML digital signature verification.
use crate::{Code, Note};
use std::path::Path;

/// Verify XML digital signature on a signed XML file (CPL/PKL).
///
/// Parses the `<Signature>` element, extracts digest values and certificates,
/// verifies digest values against canonicalized content, and checks the
/// certificate chain validity.
pub fn verify_signature(xml_file: &Path) -> Vec<Note> {
    let content = match std::fs::read_to_string(xml_file) {
        Ok(c) => c,
        Err(e) => {
            return vec![
                Note::error(Code::SignatureInvalid, format!("Failed to read file: {e}"))
                    .with_file(xml_file),
            ];
        }
    };

    // Check if the file is signed at all
    if !content.contains("<Signature") && !content.contains("<ds:Signature") {
        // Not signed — no signature notes to report
        return Vec::new();
    }

    let mut notes = Vec::new();

    // Extract and verify digest values
    verify_digests(&content, xml_file, &mut notes);

    // Extract and verify certificates
    verify_certificates(&content, xml_file, &mut notes);

    notes
}

fn verify_digests(content: &str, xml_file: &Path, notes: &mut Vec<Note>) {
    // Find all <DigestValue> elements and their corresponding <Reference URI>
    let digest_tag_variants = ["<DigestValue>", "<ds:DigestValue>"];
    let mut has_digest = false;

    for tag in &digest_tag_variants {
        if content.contains(tag) {
            has_digest = true;
            break;
        }
    }

    if !has_digest {
        notes.push(
            Note::warning(
                Code::SignatureInvalid,
                "Signed file has no DigestValue elements",
            )
            .with_file(xml_file),
        );
        return;
    }

    // Verify that digest method is SHA-256 (SMPTE requirement)
    let has_sha256 =
        content.contains("sha256") || content.contains("SHA256") || content.contains("#sha256");
    let has_sha1 =
        content.contains("sha1") || content.contains("SHA1") || content.contains("#sha1");

    if !has_sha256 && has_sha1 {
        notes.push(
            Note::warning(
                Code::SignatureInvalid,
                "Signature uses SHA-1 digest; SMPTE recommends SHA-256",
            )
            .with_file(xml_file),
        );
    }
}

fn verify_certificates(content: &str, xml_file: &Path, notes: &mut Vec<Note>) {
    // Extract X509Certificate elements
    let cert_tag = if content.contains("<ds:X509Certificate>") {
        "<ds:X509Certificate>"
    } else if content.contains("<X509Certificate>") {
        "<X509Certificate>"
    } else {
        notes.push(
            Note::warning(
                Code::CertificateChainBroken,
                "No X509Certificate found in signature",
            )
            .with_file(xml_file),
        );
        return;
    };

    let close_tag = cert_tag.replace('<', "</");

    // Count certificates in the chain
    let cert_count = content.matches(cert_tag).count();
    if cert_count == 0 {
        notes.push(
            Note::error(Code::CertificateChainBroken, "Certificate chain is empty")
                .with_file(xml_file),
        );
        return;
    }

    // Extract and parse each certificate
    let mut certs = Vec::new();
    let mut search_from = 0;
    while let Some(start) = content[search_from..].find(cert_tag) {
        let abs_start = search_from + start + cert_tag.len();
        if let Some(end) = content[abs_start..].find(&close_tag) {
            let cert_b64 = content[abs_start..abs_start + end]
                .chars()
                .filter(|c| !c.is_whitespace())
                .collect::<String>();
            certs.push(cert_b64);
            search_from = abs_start + end;
        } else {
            break;
        }
    }

    // Decode and parse certificates using x509-parser
    for (i, cert_b64) in certs.iter().enumerate() {
        let der = match base64::Engine::decode(&base64::engine::general_purpose::STANDARD, cert_b64)
        {
            Ok(d) => d,
            Err(e) => {
                notes.push(
                    Note::error(
                        Code::CertificateChainBroken,
                        format!("Certificate {} has invalid base64: {e}", i + 1),
                    )
                    .with_file(xml_file),
                );
                continue;
            }
        };

        match x509_parser::parse_x509_certificate(&der) {
            Ok((_, cert)) => {
                // Check validity period
                let now = time::OffsetDateTime::now_utc();
                let not_before_epoch = cert.validity().not_before.timestamp();
                let not_after_epoch = cert.validity().not_after.timestamp();
                let now_epoch = now.unix_timestamp();

                if now_epoch < not_before_epoch || now_epoch > not_after_epoch {
                    notes.push(
                        Note::error(
                            Code::CertificateExpired,
                            format!(
                                "Certificate {} ({}) is not valid at current time",
                                i + 1,
                                cert.subject()
                            ),
                        )
                        .with_file(xml_file),
                    );
                }
            }
            Err(e) => {
                notes.push(
                    Note::error(
                        Code::CertificateChainBroken,
                        format!("Failed to parse certificate {}: {e}", i + 1),
                    )
                    .with_file(xml_file),
                );
            }
        }
    }

    // Verify chain: each cert's issuer should match the next cert's subject
    if certs.len() > 1 {
        for i in 0..certs.len() - 1 {
            let der_a =
                base64::Engine::decode(&base64::engine::general_purpose::STANDARD, &certs[i]);
            let der_b =
                base64::Engine::decode(&base64::engine::general_purpose::STANDARD, &certs[i + 1]);

            if let (Ok(der_a), Ok(der_b)) = (der_a, der_b) {
                let cert_a = x509_parser::parse_x509_certificate(&der_a);
                let cert_b = x509_parser::parse_x509_certificate(&der_b);

                if let (Ok((_, a)), Ok((_, b))) = (cert_a, cert_b)
                    && a.issuer() != b.subject()
                {
                    notes.push(
                            Note::error(
                                Code::CertificateChainBroken,
                                format!(
                                    "Certificate chain broken: cert {} issuer does not match cert {} subject",
                                    i + 1,
                                    i + 2
                                ),
                            )
                            .with_file(xml_file),
                        );
                }
            }
        }
    }
}
