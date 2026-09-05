// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

// This file is the companion protocol implementation
// can change anytime and is still WIP

use anyhow::{Context, Result, anyhow, bail};
use idevice::Idevice;
use serde_json::{Value, json};
use uuid::Uuid;

pub const COMPANION_PORT: u16 = 52_848;
pub const HEADER_LEN: usize = 24;
const MAGIC: &[u8; 4] = b"IDCP";
const PROTOCOL_MAJOR: u8 = 1;
const MAX_CONTROL_PAYLOAD: usize = 256 * 1024;

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Service {
    Core = 0x00,
    GalleryImport = 0x01,
    Webcam = 0x02,
    FileTransfer = 0x03,
}

impl TryFrom<u8> for Service {
    type Error = anyhow::Error;

    fn try_from(value: u8) -> Result<Self> {
        match value {
            0x00 => Ok(Self::Core),
            0x01 => Ok(Self::GalleryImport),
            0x02 => Ok(Self::Webcam),
            0x03 => Ok(Self::FileTransfer),
            _ => bail!("unknown companion service {value:#04x}"),
        }
    }
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CoreOpcode {
    Hello = 0x01,
    HelloAck = 0x02,
    Ping = 0x03,
    Pong = 0x04,
    Error = 0x05,
    EventAck = 0x06,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum GalleryImportOpcode {
    Event = 0x01,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum GalleryImportEventKind {
    ImportServiceReady = 0x01,
    BatchNeedsAuthorization = 0x02,
    BatchStarted = 0x03,
    ItemSucceeded = 0x04,
    ItemFailed = 0x05,
    BatchSucceeded = 0x06,
    BatchPartiallySucceeded = 0x07,
    BatchFailed = 0x08,
    BatchCancelled = 0x09,
}

impl GalleryImportEventKind {
    pub fn from_json(value: &Value) -> Result<Self> {
        match value.as_u64() {
            Some(0x01) => Ok(Self::ImportServiceReady),
            Some(0x02) => Ok(Self::BatchNeedsAuthorization),
            Some(0x03) => Ok(Self::BatchStarted),
            Some(0x04) => Ok(Self::ItemSucceeded),
            Some(0x05) => Ok(Self::ItemFailed),
            Some(0x06) => Ok(Self::BatchSucceeded),
            Some(0x07) => Ok(Self::BatchPartiallySucceeded),
            Some(0x08) => Ok(Self::BatchFailed),
            Some(0x09) => Ok(Self::BatchCancelled),
            _ => bail!("unknown gallery import event kind"),
        }
    }

    pub fn is_terminal(self) -> bool {
        matches!(
            self,
            Self::BatchSucceeded
                | Self::BatchPartiallySucceeded
                | Self::BatchFailed
                | Self::BatchCancelled
        )
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Header {
    pub service: Service,
    pub opcode: u8,
    pub payload_len: u32,
    pub sequence: u64,
}

impl Header {
    pub fn encode(&self) -> [u8; HEADER_LEN] {
        let mut out = [0_u8; HEADER_LEN];
        out[..4].copy_from_slice(MAGIC);
        out[4] = PROTOCOL_MAJOR;
        out[5] = 0;
        out[6] = self.service as u8;
        out[7] = self.opcode;
        out[12..16].copy_from_slice(&self.payload_len.to_be_bytes());
        out[16..24].copy_from_slice(&self.sequence.to_be_bytes());
        out
    }

    pub fn decode(bytes: &[u8]) -> Result<Self> {
        if bytes.len() != HEADER_LEN {
            bail!("invalid companion header length");
        }
        if &bytes[..4] != MAGIC {
            bail!("invalid companion protocol magic");
        }
        if bytes[4] != PROTOCOL_MAJOR {
            bail!("unsupported companion protocol version {}", bytes[4]);
        }
        if bytes[8..12] != [0, 0, 0, 0] {
            bail!("unsupported companion protocol flags");
        }
        let payload_len = u32::from_be_bytes(bytes[12..16].try_into().unwrap());
        if payload_len as usize > MAX_CONTROL_PAYLOAD {
            bail!("companion control payload is too large");
        }
        Ok(Self {
            service: Service::try_from(bytes[6])?,
            opcode: bytes[7],
            payload_len,
            sequence: u64::from_be_bytes(bytes[16..24].try_into().unwrap()),
        })
    }
}

#[derive(Clone, Debug)]
pub struct GalleryEvent {
    pub kind: GalleryImportEventKind,
    pub batch_id: Option<Uuid>,
    pub revision: u64,
    pub completed_items: u64,
    pub failed_items: u64,
    pub total_items: u64,
    pub message: Option<String>,
}

pub struct Session {
    stream: Idevice,
    outgoing_sequence: u64,
    incoming_sequence: u64,
}

impl Session {
    pub fn new(stream: Idevice) -> Self {
        Self {
            stream,
            outgoing_sequence: 1,
            incoming_sequence: 1,
        }
    }

    pub async fn send_hello(&mut self, instance_id: Uuid) -> Result<()> {
        let payload = json!({
            "instanceID": instance_id.to_string(),
            "displayName": "iDescriptor Desktop",//
            "applicationVersion": env!("CARGO_PKG_VERSION"),
            "platform": desktop_platform(),
            "capabilities": 3_u64
        });
        self.send(Service::Core, CoreOpcode::Hello as u8, &payload)
            .await
    }

    pub async fn next_gallery_event(
        &mut self,
        expected_batch: Uuid,
        expected_client: Uuid,
    ) -> Result<GalleryEvent> {
        loop {
            let (header, payload) = self.receive().await?;
            match (header.service, header.opcode) {
                (Service::Core, opcode) if opcode == CoreOpcode::HelloAck as u8 => {}
                (Service::Core, opcode) if opcode == CoreOpcode::Ping as u8 => {
                    self.send_bytes(Service::Core, CoreOpcode::Pong as u8, &payload)
                        .await?;
                }
                (Service::Core, opcode) if opcode == CoreOpcode::Error as u8 => {
                    let value: Value = serde_json::from_slice(&payload)
                        .context("invalid companion error payload")?;
                    bail!(
                        "companion protocol error: {}",
                        value["message"].as_str().unwrap_or("unknown error")
                    );
                }
                (Service::GalleryImport, opcode) if opcode == GalleryImportOpcode::Event as u8 => {
                    let value: Value =
                        serde_json::from_slice(&payload).context("invalid gallery import event")?;
                    let target = value["targetClientInstanceID"]
                        .as_str()
                        .and_then(|value| Uuid::parse_str(value).ok());
                    let batch_id = value["batchID"]
                        .as_str()
                        .and_then(|value| Uuid::parse_str(value).ok());
                    if batch_id.is_some() && batch_id != Some(expected_batch) {
                        continue;
                    }
                    if target != Some(expected_client) {
                        continue;
                    }
                    let event = GalleryEvent {
                        kind: GalleryImportEventKind::from_json(&value["kind"])?,
                        batch_id,
                        revision: value["revision"].as_u64().unwrap_or(0),
                        completed_items: value["completedItems"].as_u64().unwrap_or(0),
                        failed_items: value["failedItems"].as_u64().unwrap_or(0),
                        total_items: value["totalItems"].as_u64().unwrap_or(0),
                        message: value["message"].as_str().map(str::to_owned),
                    };
                    // Service-ready events have no batch and are useful to callers too.
                    return Ok(event);
                }
                _ => return Err(anyhow!("unexpected companion protocol message")),
            }
        }
    }

    pub async fn acknowledge(&mut self, batch_id: Uuid, revision: u64) -> Result<()> {
        self.send(
            Service::Core,
            CoreOpcode::EventAck as u8,
            &json!({ "batchID": batch_id.to_string(), "revision": revision }),
        )
        .await
    }

    async fn receive(&mut self) -> Result<(Header, Vec<u8>)> {
        let header_bytes = self
            .stream
            .read_raw(HEADER_LEN)
            .await
            .context("failed to read companion header")?;
        let header = Header::decode(&header_bytes)?;
        if header.sequence != self.incoming_sequence {
            bail!(
                "companion sequence mismatch: expected {}, received {}",
                self.incoming_sequence,
                header.sequence
            );
        }
        self.incoming_sequence += 1;
        let payload = self
            .stream
            .read_raw(header.payload_len as usize)
            .await
            .context("failed to read companion payload")?;
        Ok((header, payload))
    }

    async fn send(&mut self, service: Service, opcode: u8, value: &Value) -> Result<()> {
        let payload = serde_json::to_vec(value)?;
        self.send_bytes(service, opcode, &payload).await
    }

    async fn send_bytes(&mut self, service: Service, opcode: u8, payload: &[u8]) -> Result<()> {
        if payload.len() > MAX_CONTROL_PAYLOAD {
            bail!("companion control payload is too large");
        }
        let header = Header {
            service,
            opcode,
            payload_len: payload.len() as u32,
            sequence: self.outgoing_sequence,
        };
        self.outgoing_sequence += 1;
        let mut message = Vec::with_capacity(HEADER_LEN + payload.len());
        message.extend_from_slice(&header.encode());
        message.extend_from_slice(payload);
        self.stream
            .send_raw(&message)
            .await
            .context("failed to write companion message")
    }
}

fn desktop_platform() -> u8 {
    if cfg!(target_os = "macos") {
        0x02
    } else if cfg!(target_os = "windows") {
        0x03
    } else {
        0x04
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn header_matches_swift_golden_bytes() {
        let header = Header {
            service: Service::GalleryImport,
            opcode: GalleryImportOpcode::Event as u8,
            payload_len: 0x102,
            sequence: 0x0102_0304_0506_0708,
        };
        assert_eq!(
            header.encode(),
            [
                0x49, 0x44, 0x43, 0x50, 0x01, 0x00, 0x01, 0x01, 0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 3, 4,
                5, 6, 7, 8,
            ]
        );
        assert_eq!(Header::decode(&header.encode()).unwrap(), header);
    }
}
