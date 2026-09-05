// SPDX-FileCopyrightText: 2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: GPL-3.0-or-later

//! A reusable Rust AirPlay receiver library.
//!
//! The initial implementation provides the DNS-SD advertisements needed for
//! AirPlay and RAOP discovery. Protocol and media subsystems will be added
//! without changing the discovery API.

pub mod discovery;
pub mod pairing;
pub mod playback;
pub mod receiver;

pub use pairing::PersistentPairingStore;
pub use playback::GstreamerPlayback;
pub use receiver::{Receiver, ReceiverConfig, ReceiverEvent};
