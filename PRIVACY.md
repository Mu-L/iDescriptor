# Privacy Policy for iDescriptor

iDescriptor ("we", "us", or "the project") is an open-source, client-side application designed to manage and interact with Apple mobile devices (iPhone, iPad, iPod touch). We are committed to transparency and protecting your privacy. This policy explains what data is handled and how it is protected when you use iDescriptor.

## 1. Core Principle: No Tracking or Remote Data Collection

* iDescriptor **does not** collect, store, transmit, or sell any personal data or telemetry.
* There are no embedded third-party analytics, tracking SDKs, advertising frameworks, or user profiling tools.

## 2. Local Data Processing

All core features of iDescriptor execute strictly on your local computer:
* **Device Information:** Data read from your connected device (such as UDID, serial number, IMEI, hardware specifications, diagnostics, and battery health) is processed in memory solely to display it in the user interface.
* **Photos, Files, and Backups:** Any files exported, browsed, or backed up are stored locally on your machine at the destination path you select. None of this data is ever sent to external servers.

## 3. Network Connections and Third-Party Services

iDescriptor only initiates network connections in specific, user-triggered scenarios:

### a. Apple App Store & Authentication (via ipatool)
When you choose to use the App management features requiring Apple ID authentication:
* Your Apple ID credentials (email, password, and two-factor authentication codes) are transmitted **directly to Apple's official servers** using secure HTTPS connections.
* Your credentials are never transmitted to, inspected by, or stored on any server controlled by iDescriptor.
* Session tokens and search caches are kept locally on your machine to maintain your session.

### b. Software Updates
iDescriptor may check for application updates by contacting GitHub's servers (GitHub Releases) or the official project website. These requests only transmit standard HTTP connection data (such as your IP address and client user-agent), which are handled under [GitHub's Privacy Statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).

### c. Local Network Features (AirPlay & Wi-Fi Sync)
AirPlay screen mirroring and Wi-Fi device communication operate strictly within your local area network (LAN) using Bonjour / Avahi / Zeroconf and do not route device traffic over the public internet.

## 4. Third-Party Distribution Platforms

If you download iDescriptor through third-party platforms such as Flathub, Arch User Repository (AUR), NixOS, or package managers, the download and package distribution are subject to the privacy policies of those respective services.

## 5. Changes to This Policy

We may update this Privacy Policy from time to time as features evolve. Any changes will be reflected in this document with an updated effective date.

## 6. Contact & Questions

If you have questions about this Privacy Policy or iDescriptor's data handling practices, please open an issue on GitHub:
* [iDescriptor GitHub Issues](https://github.com/iDescriptor/iDescriptor/issues)
