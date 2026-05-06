#!/usr/bin/env python3
"""Create a minimal valid SMPTE DCP for testing."""

import os
import hashlib
import base64
import uuid

OUTDIR = "tests/fixtures/valid_smpte"
os.makedirs(OUTDIR, exist_ok=True)

# Create a minimal MXF-like file (just some bytes for hash testing)
picture_data = b"FAKE_MXF_PICTURE_DATA_" + os.urandom(64)
sound_data = b"FAKE_MXF_SOUND_DATA_" + os.urandom(64)

picture_uuid = "urn:uuid:" + str(uuid.uuid4())
sound_uuid = "urn:uuid:" + str(uuid.uuid4())
cpl_uuid = "urn:uuid:" + str(uuid.uuid4())
pkl_uuid = "urn:uuid:" + str(uuid.uuid4())
am_uuid = "urn:uuid:" + str(uuid.uuid4())
reel_uuid = "urn:uuid:" + str(uuid.uuid4())

# Write picture file
picture_file = "picture.mxf"
with open(os.path.join(OUTDIR, picture_file), "wb") as f:
    f.write(picture_data)

# Write sound file
sound_file = "sound.mxf"
with open(os.path.join(OUTDIR, sound_file), "wb") as f:
    f.write(sound_data)

# Compute hashes
def sha1_b64(data):
    return base64.b64encode(hashlib.sha1(data).digest()).decode()

picture_hash = sha1_b64(picture_data)
sound_hash = sha1_b64(sound_data)
picture_size = len(picture_data)
sound_size = len(sound_data)

# Write CPL
cpl_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<CompositionPlaylist xmlns="http://www.smpte-ra.org/schemas/429-7/2006/CPL">
  <Id>{cpl_uuid}</Id>
  <ContentTitleText>Test DCP</ContentTitleText>
  <IssueDate>2024-01-01T00:00:00+00:00</IssueDate>
  <ContentKind>test</ContentKind>
  <ReelList>
    <Reel>
      <Id>{reel_uuid}</Id>
      <AssetList>
        <MainPicture>
          <Id>{picture_uuid}</Id>
          <EditRate>24 1</EditRate>
          <IntrinsicDuration>100</IntrinsicDuration>
          <Duration>100</Duration>
          <EntryPoint>0</EntryPoint>
          <FrameRate>24 1</FrameRate>
          <ScreenAspectRatio>1998 1080</ScreenAspectRatio>
        </MainPicture>
        <MainSound>
          <Id>{sound_uuid}</Id>
          <EditRate>24 1</EditRate>
          <IntrinsicDuration>100</IntrinsicDuration>
          <Duration>100</Duration>
          <EntryPoint>0</EntryPoint>
        </MainSound>
      </AssetList>
    </Reel>
  </ReelList>
</CompositionPlaylist>
"""
cpl_file = "cpl.xml"
with open(os.path.join(OUTDIR, cpl_file), "w") as f:
    f.write(cpl_xml)

cpl_data = cpl_xml.encode()
cpl_hash = sha1_b64(cpl_data)
cpl_size = len(cpl_data)

# Write PKL
pkl_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<PackingList xmlns="http://www.smpte-ra.org/schemas/429-8/2007/PKL">
  <Id>{pkl_uuid}</Id>
  <Creator>dcpdoctor test fixture generator</Creator>
  <IssueDate>2024-01-01T00:00:00+00:00</IssueDate>
  <AssetList>
    <Asset>
      <Id>{picture_uuid}</Id>
      <Hash>{picture_hash}</Hash>
      <Size>{picture_size}</Size>
      <Type>application/mxf</Type>
      <OriginalFileName>{picture_file}</OriginalFileName>
    </Asset>
    <Asset>
      <Id>{sound_uuid}</Id>
      <Hash>{sound_hash}</Hash>
      <Size>{sound_size}</Size>
      <Type>application/mxf</Type>
      <OriginalFileName>{sound_file}</OriginalFileName>
    </Asset>
    <Asset>
      <Id>{cpl_uuid}</Id>
      <Hash>{cpl_hash}</Hash>
      <Size>{cpl_size}</Size>
      <Type>text/xml</Type>
      <OriginalFileName>{cpl_file}</OriginalFileName>
    </Asset>
  </AssetList>
</PackingList>
"""
pkl_file = "pkl.xml"
with open(os.path.join(OUTDIR, pkl_file), "w") as f:
    f.write(pkl_xml)

# Write ASSETMAP.xml (SMPTE style)
assetmap_xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<AssetMap xmlns="http://www.smpte-ra.org/schemas/429-9/2007/AM">
  <Id>{am_uuid}</Id>
  <Creator>dcpdoctor test fixture generator</Creator>
  <IssueDate>2024-01-01T00:00:00+00:00</IssueDate>
  <AssetList>
    <Asset>
      <Id>{pkl_uuid}</Id>
      <PackingList>true</PackingList>
      <ChunkList>
        <Chunk>
          <Path>{pkl_file}</Path>
        </Chunk>
      </ChunkList>
    </Asset>
    <Asset>
      <Id>{cpl_uuid}</Id>
      <ChunkList>
        <Chunk>
          <Path>{cpl_file}</Path>
        </Chunk>
      </ChunkList>
    </Asset>
    <Asset>
      <Id>{picture_uuid}</Id>
      <ChunkList>
        <Chunk>
          <Path>{picture_file}</Path>
        </Chunk>
      </ChunkList>
    </Asset>
    <Asset>
      <Id>{sound_uuid}</Id>
      <ChunkList>
        <Chunk>
          <Path>{sound_file}</Path>
        </Chunk>
      </ChunkList>
    </Asset>
  </AssetList>
</AssetMap>
"""
with open(os.path.join(OUTDIR, "ASSETMAP.xml"), "w") as f:
    f.write(assetmap_xml)

print(f"Created test DCP in {OUTDIR}/")
print(f"  Picture: {picture_file} ({picture_size} bytes, hash={picture_hash})")
print(f"  Sound:   {sound_file} ({sound_size} bytes, hash={sound_hash})")
print(f"  CPL:     {cpl_file} ({cpl_size} bytes)")
print(f"  PKL:     {pkl_file}")
print(f"  ASSETMAP.xml")

# Also create a broken DCP for negative testing
BADDIR = "tests/fixtures/bad_hash"
os.makedirs(BADDIR, exist_ok=True)

# Copy ASSETMAP and PKL but corrupt the picture file
import shutil
shutil.copy(os.path.join(OUTDIR, "ASSETMAP.xml"), os.path.join(BADDIR, "ASSETMAP.xml"))
shutil.copy(os.path.join(OUTDIR, pkl_file), os.path.join(BADDIR, pkl_file))
shutil.copy(os.path.join(OUTDIR, cpl_file), os.path.join(BADDIR, cpl_file))
shutil.copy(os.path.join(OUTDIR, sound_file), os.path.join(BADDIR, sound_file))

# Write corrupted picture
with open(os.path.join(BADDIR, picture_file), "wb") as f:
    f.write(b"CORRUPTED_DATA")

print(f"\nCreated bad-hash DCP in {BADDIR}/")
