#pragma once

#include <stddef.h>
#include <stdint.h>

namespace product_error {

enum class Code : uint16_t {
    None = 0,
    GpioInitFailed = 1,
    EventQueueAllocationFailed = 2,
    BleInitFailed = 3,
    BleClientSetupFailed = 4,
    NvsOpenFailed = 5,
    BindingWithoutBond = 6,
    BondCleanupAtStartFailed = 7,
    PairScanAbortStopFailed = 8,
    UnconfirmedBondDeleteFailed = 9,
    ScanStopBeforePairFailed = 10,
    OldBondCleanupFailed = 11,
    PostConfirmBondCleanupFailed = 12,
    ScanStopBeforeClearFailed = 13,
    ClearMarkerSaveFailed = 14,
    ClearBondDeleteFailed = 15,
    PostClearBondCleanupFailed = 16,
    EventQueueOverflow = 17,
    ReconnectClientDisconnectPending = 18,
    ReconnectClientAlreadyConnected = 19,
    ReconnectConnectFailed = 20,
    ReconnectSecurityFailed = 21,
    ReconnectLinkNotEncrypted = 22,
    ReconnectLinkNotBonded = 23,
    ReconnectPeerIdMismatch = 24,
    ReconnectLocalBondMissingAfterSecurity = 25,
    ReconnectEndpointAmbiguous = 26,
    ReconnectCccdMissing = 27,
    ReconnectSubscriptionRejected = 28,
    ReconnectLocalBondMissing = 29,
};

constexpr size_t kErrorCount = 30;

static constexpr const char* const kErrorTexts[kErrorCount] = {
    "",                                                                  /* 0  None */
    "GPIO8 konnte nicht sicher initialisiert werden",                   /* 1  GpioInitFailed */
    "Eventqueue fehlt",                                                 /* 2  EventQueueAllocationFailed */
    "NimBLE-Initialisierung fehlgeschlagen",                            /* 3  BleInitFailed */
    "BLE-Scanner oder Client konnte nicht angelegt werden",             /* 4  BleClientSetupFailed */
    "NVS konnte nicht geöffnet werden",                                 /* 5  NvsOpenFailed */
    "Binding ohne passenden Bond",                                      /* 6  BindingWithoutBond */
    "Bonds konnten beim Start nicht bereinigt werden",                  /* 7  BondCleanupAtStartFailed */
    "Pairing-Scan konnte nicht sicher stoppen",                         /* 8  PairScanAbortStopFailed */
    "Unbestätigter Bond konnte nicht gelöscht werden",                  /* 9  UnconfirmedBondDeleteFailed */
    "Scan konnte vor Pairing nicht stoppen",                            /* 10 ScanStopBeforePairFailed */
    "Alte Bonds konnten nicht bereinigt werden",                        /* 11 OldBondCleanupFailed */
    "Alte Bonds konnten nach Bestätigung nicht bereinigt werden",       /* 12 PostConfirmBondCleanupFailed */
    "Scan konnte vor Löschen nicht stoppen",                            /* 13 ScanStopBeforeClearFailed */
    "Löschmarkierung konnte nicht gespeichert werden",                  /* 14 ClearMarkerSaveFailed */
    "Zuordnung gelöscht, Bond-Löschung fehlgeschlagen",                 /* 15 ClearBondDeleteFailed */
    "Zuordnung gelöscht, Altbonds konnten nicht bereinigt werden",      /* 16 PostClearBondCleanupFailed */
    "Eventqueue überlaufen",                                            /* 17 EventQueueOverflow */
    "Client vor Reconnect noch im Trennen; späterer Retry",             /* 18 ReconnectClientDisconnectPending */
    "Client vor Reconnect noch verbunden; Trennung und späterer Retry", /* 19 ReconnectClientAlreadyConnected */
    "Connect zur gespeicherten ID fehlgeschlagen",                      /* 20 ReconnectConnectFailed */
    "Security fehlgeschlagen",                                          /* 21 ReconnectSecurityFailed */
    "Link nicht verschlüsselt",                                         /* 22 ReconnectLinkNotEncrypted */
    "Link nicht gebondet",                                              /* 23 ReconnectLinkNotBonded */
    "Peer-ID weicht von gespeicherter ID ab",                           /* 24 ReconnectPeerIdMismatch */
    "Lokaler Bond für gespeicherte ID fehlt nach Security",             /* 25 ReconnectLocalBondMissingAfterSecurity */
    "Gespeicherter GATT-/HID-Endpunkt nicht eindeutig gefunden",        /* 26 ReconnectEndpointAmbiguous */
    "CCCD 0x2902 am gespeicherten Endpunkt fehlt",                      /* 27 ReconnectCccdMissing */
    "CCCD-Subscription abgelehnt",                                      /* 28 ReconnectSubscriptionRejected */
    "Lokaler Bond für gespeicherte ID fehlt vor Reconnect",             /* 29 ReconnectLocalBondMissing */
};

static_assert(sizeof(kErrorTexts) / sizeof(kErrorTexts[0]) == kErrorCount, "Arraynummern müssen zum Fehlercode passen");

inline uint16_t number(Code code) {
    return static_cast<uint16_t>(code);
}

inline const char* text(Code code) {
    const uint16_t index = number(code);
    return index < kErrorCount ? kErrorTexts[index] : "Unbekannter Fehler";
}

} // namespace product_error