/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_8.h"

#include <string>

/**
 * @brief Client proxy for protocol v1.9 (cross-device drag-and-drop file transfer)
 *
 * Extends v1.8 by wiring the drag/file-transfer protocol frames that older
 * proxies (1.5-1.8) declared but left as no-op stubs. This is the first
 * proxy version that actually writes DDRG / DFTR / DFTM / QDTI / QDGX / DFPR
 * on the wire and dispatches inbound ones to the Server.
 *
 * @since Protocol version 1.9
 */
class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  // -- Outbound (Server → Client) ------------------------------------------

  //! Send the drag manifest (DDRG) for a new transfer session
  void sendDragInfo(uint32_t fileCount, const char *info, size_t size) override;

  //! Send a single file-chunk frame (DFTR).
  //! `data` layout: `[transferId:4][mark:1==inline-mark ? unused : payload]`.
  //! The v1.9 sender packs `transferId` into the leading 4 bytes of `data`.
  void fileChunkSending(uint8_t mark, char *data, size_t dataSize) override;

  //! Send per-file metadata (DFTM)
  void sendFileTransferMeta(
      uint32_t transferId, uint32_t fileIndex, uint64_t size, uint8_t flags, const std::string &relPath
  ) override;

  //! Ask the peer where the drop should land (QDTI query)
  void sendDropTargetQuery(uint32_t transferId, int16_t hoverX, int16_t hoverY) override;

  //! Abort an in-progress transfer (QDGX)
  void sendDragCancel(uint32_t transferId, uint8_t reason) override;

  //! Acknowledge incremental byte progress (DFPR, optional)
  void sendFileTransferProgress(uint32_t transferId, uint32_t fileIndex, uint64_t done, uint32_t percent) override;

  // -- Inbound (dispatched by parseMessage) --------------------------------

  bool parseMessage(const uint8_t *code) override;

private:
  void handleDragInfo();
  void handleFileTransferMeta();
  void handleFileChunk();
  void handleDropTargetInfo(); ///< QDTI reply from peer
  void handleDragCancel();
  void handleFileTransferProgress();
};
