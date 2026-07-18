/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include "server/Server.h"

#include <cstring>

//
// ClientProxy1_9 -- Stage 1 skeleton
//
// All wire-level frames are encoded/decoded here, but the actual state
// machine (FileTransferManager) is wired up in Stage 3. For now every
// handler logs the parsed fields and forwards nothing.
//

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
  LOG_DEBUG("client \"%s\" negotiated protocol v1.9 (drag-and-drop file transfer enabled)", name.c_str());
}

// ---------------------------------------------------------------------------
// Outbound
// ---------------------------------------------------------------------------

void ClientProxy1_9::sendDragInfo(uint32_t fileCount, const char *info, size_t size)
{
  // The DDRG frame in v1.9 carries: fileCount, transferId, totalSizeHi,
  // totalSizeLo, manifest_blob. In Stage 1 we don't yet own a
  // FileTransferManager, so callers pass a pre-serialized manifest via
  // `info`/`size` and set `fileCount` to the number of entries. transferId
  // and total size are decoded from the leading 12 bytes of `info` by
  // convention: [transferId:4][totalSizeHi:4][totalSizeLo:4][manifest...].
  //
  // Callers that don't have a header (e.g. legacy call sites) may pass
  // `size < 12`; in that case we send zeros for the header and the entire
  // buffer as the manifest.
  uint32_t transferId = 0;
  uint32_t totalHi = 0;
  uint32_t totalLo = 0;
  const char *manifest = info;
  size_t manifestSize = size;

  if (info != nullptr && size >= 12) {
    std::memcpy(&transferId, info + 0, 4);
    std::memcpy(&totalHi, info + 4, 4);
    std::memcpy(&totalLo, info + 8, 4);
    manifest = info + 12;
    manifestSize = size - 12;
  }

  std::string manifestStr(manifest, manifestSize);
  LOG_DEBUG(
      "send drag info to \"%s\": count=%u xferId=%u totalSize=%llu manifestBytes=%zu", getName().c_str(),
      static_cast<unsigned>(fileCount), static_cast<unsigned>(transferId),
      (static_cast<unsigned long long>(totalHi) << 32) | totalLo, manifestSize
  );
  // Note: ProtocolUtil::writef pulls all integer arguments via va_arg(uint32_t),
  // so we must promote every parameter to uint32_t explicitly (even the %2i
  // ones) to avoid stack-width mismatches on some ABIs.
  ProtocolUtil::writef(
      getStream(), kMsgDDragInfo, static_cast<uint32_t>(fileCount), transferId, totalHi, totalLo, &manifestStr
  );
}

void ClientProxy1_9::fileChunkSending(uint8_t mark, char *data, size_t dataSize)
{
  std::string payload(data, dataSize);
  LOG_DEBUG("send file chunk to \"%s\": mark=%u bytes=%zu", getName().c_str(), static_cast<unsigned>(mark), dataSize);
  ProtocolUtil::writef(getStream(), kMsgDFileTransfer, static_cast<uint32_t>(mark), &payload);
}

void ClientProxy1_9::sendFileTransferMeta(
    uint32_t transferId, uint32_t fileIndex, uint64_t size, uint8_t flags, const std::string &relPath
)
{
  uint32_t sizeHi = static_cast<uint32_t>(size >> 32);
  uint32_t sizeLo = static_cast<uint32_t>(size & 0xFFFFFFFFu);
  LOG_DEBUG(
      "send file meta to \"%s\": xferId=%u idx=%u size=%llu flags=0x%02x path=%s", getName().c_str(),
      static_cast<unsigned>(transferId), static_cast<unsigned>(fileIndex),
      static_cast<unsigned long long>(size), static_cast<unsigned>(flags), relPath.c_str()
  );
  ProtocolUtil::writef(
      getStream(), kMsgDFileTransferMeta, transferId, fileIndex, sizeHi, sizeLo, static_cast<uint32_t>(flags), &relPath
  );
}

void ClientProxy1_9::sendDropTargetQuery(uint32_t transferId, int16_t hoverX, int16_t hoverY)
{
  LOG_DEBUG(
      "send drop-target query to \"%s\": xferId=%u hover=(%d,%d)", getName().c_str(),
      static_cast<unsigned>(transferId), hoverX, hoverY
  );
  // int16_t → uint32_t sign-extended low bits; %2i writer will truncate to 16.
  ProtocolUtil::writef(
      getStream(), kMsgQDropTargetInfoQuery, transferId,
      static_cast<uint32_t>(static_cast<uint16_t>(hoverX)),
      static_cast<uint32_t>(static_cast<uint16_t>(hoverY))
  );
}

void ClientProxy1_9::sendDragCancel(uint32_t transferId, uint8_t reason)
{
  LOG_DEBUG(
      "send drag cancel to \"%s\": xferId=%u reason=%u", getName().c_str(), static_cast<unsigned>(transferId),
      static_cast<unsigned>(reason)
  );
  ProtocolUtil::writef(getStream(), kMsgQDragCancel, transferId, static_cast<uint32_t>(reason));
}

void ClientProxy1_9::sendFileTransferProgress(uint32_t transferId, uint32_t fileIndex, uint64_t done, uint32_t percent)
{
  uint32_t doneHi = static_cast<uint32_t>(done >> 32);
  uint32_t doneLo = static_cast<uint32_t>(done & 0xFFFFFFFFu);
  LOG_DEBUG(
      "send progress to \"%s\": xferId=%u idx=%u done=%llu percent=%u.%02u", getName().c_str(),
      static_cast<unsigned>(transferId), static_cast<unsigned>(fileIndex),
      static_cast<unsigned long long>(done), percent / 100, percent % 100
  );
  ProtocolUtil::writef(getStream(), kMsgDFileTransferProgress, transferId, fileIndex, doneHi, doneLo, percent);
}

// ---------------------------------------------------------------------------
// Inbound
// ---------------------------------------------------------------------------

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (std::memcmp(code, kMsgDDragInfo, 4) == 0) {
    handleDragInfo();
  } else if (std::memcmp(code, kMsgDFileTransferMeta, 4) == 0) {
    handleFileTransferMeta();
  } else if (std::memcmp(code, kMsgDFileTransfer, 4) == 0) {
    handleFileChunk();
  } else if (std::memcmp(code, kMsgQDropTargetInfo, 4) == 0) {
    handleDropTargetInfo();
  } else if (std::memcmp(code, kMsgQDragCancel, 4) == 0) {
    handleDragCancel();
  } else if (std::memcmp(code, kMsgDFileTransferProgress, 4) == 0) {
    handleFileTransferProgress();
  } else {
    return ClientProxy1_8::parseMessage(code);
  }
  return true;
}

void ClientProxy1_9::handleDragInfo()
{
  uint16_t fileCount = 0;
  uint32_t transferId = 0;
  uint32_t totalHi = 0;
  uint32_t totalLo = 0;
  std::string manifest;
  if (!ProtocolUtil::readf(
          getStream(), kMsgDDragInfo + 4, &fileCount, &transferId, &totalHi, &totalLo, &manifest
      )) {
    LOG_WARN("failed to parse DDRG from \"%s\"", getName().c_str());
    return;
  }
  LOG_DEBUG(
      "recv drag info from \"%s\": count=%u xferId=%u totalSize=%llu manifestBytes=%zu", getName().c_str(),
      static_cast<unsigned>(fileCount), static_cast<unsigned>(transferId),
      (static_cast<unsigned long long>(totalHi) << 32) | totalLo, manifest.size()
  );
  // Stage 3: forward to Server::onIncomingDragInfo(this, transferId, count, total, manifest)
}

void ClientProxy1_9::handleFileTransferMeta()
{
  uint32_t transferId = 0;
  uint32_t fileIndex = 0;
  uint32_t sizeHi = 0;
  uint32_t sizeLo = 0;
  uint8_t flags = 0;
  std::string relPath;
  if (!ProtocolUtil::readf(
          getStream(), kMsgDFileTransferMeta + 4, &transferId, &fileIndex, &sizeHi, &sizeLo, &flags, &relPath
      )) {
    LOG_WARN("failed to parse DFTM from \"%s\"", getName().c_str());
    return;
  }
  uint64_t size = (static_cast<uint64_t>(sizeHi) << 32) | sizeLo;
  LOG_DEBUG(
      "recv file meta from \"%s\": xferId=%u idx=%u size=%llu flags=0x%02x path=%s", getName().c_str(),
      static_cast<unsigned>(transferId), static_cast<unsigned>(fileIndex),
      static_cast<unsigned long long>(size), static_cast<unsigned>(flags), relPath.c_str()
  );
  // Stage 3: FileTransferManager::onMeta(...)
}

void ClientProxy1_9::handleFileChunk()
{
  uint8_t mark = 0;
  std::string data;
  if (!ProtocolUtil::readf(getStream(), kMsgDFileTransfer + 4, &mark, &data)) {
    LOG_WARN("failed to parse DFTR from \"%s\"", getName().c_str());
    return;
  }
  LOG_DEBUG(
      "recv file chunk from \"%s\": mark=%u bytes=%zu", getName().c_str(), static_cast<unsigned>(mark), data.size()
  );
  // Stage 3: FileTransferManager::onChunk(mark, data)
}

void ClientProxy1_9::handleDropTargetInfo()
{
  uint32_t transferId = 0;
  uint8_t strategy = 0;
  std::string targetPath;
  if (!ProtocolUtil::readf(getStream(), kMsgQDropTargetInfo + 4, &transferId, &strategy, &targetPath)) {
    LOG_WARN("failed to parse QDTI reply from \"%s\"", getName().c_str());
    return;
  }
  LOG_DEBUG(
      "recv drop-target info from \"%s\": xferId=%u strategy=%u target=%s", getName().c_str(),
      static_cast<unsigned>(transferId), static_cast<unsigned>(strategy), targetPath.c_str()
  );
  // Stage 3: FileTransferManager::onDropTargetReply(transferId, strategy, targetPath)
}

void ClientProxy1_9::handleDragCancel()
{
  uint32_t transferId = 0;
  uint8_t reason = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgQDragCancel + 4, &transferId, &reason)) {
    LOG_WARN("failed to parse QDGX from \"%s\"", getName().c_str());
    return;
  }
  LOG_DEBUG(
      "recv drag cancel from \"%s\": xferId=%u reason=%u", getName().c_str(), static_cast<unsigned>(transferId),
      static_cast<unsigned>(reason)
  );
  // Stage 3: FileTransferManager::onAbort(transferId, reason)
}

void ClientProxy1_9::handleFileTransferProgress()
{
  uint32_t transferId = 0;
  uint32_t fileIndex = 0;
  uint32_t doneHi = 0;
  uint32_t doneLo = 0;
  uint32_t percent = 0;
  if (!ProtocolUtil::readf(
          getStream(), kMsgDFileTransferProgress + 4, &transferId, &fileIndex, &doneHi, &doneLo, &percent
      )) {
    LOG_WARN("failed to parse DFPR from \"%s\"", getName().c_str());
    return;
  }
  uint64_t done = (static_cast<uint64_t>(doneHi) << 32) | doneLo;
  LOG_DEBUG(
      "recv progress from \"%s\": xferId=%u idx=%u done=%llu percent=%u.%02u", getName().c_str(),
      static_cast<unsigned>(transferId), static_cast<unsigned>(fileIndex),
      static_cast<unsigned long long>(done), percent / 100, percent % 100
  );
  // Stage 5: forward to GUI via IPC
}
