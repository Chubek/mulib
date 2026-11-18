file(SHA256 "${PROGRAM}" BINARY_HASH)
file(SHA256 "${SCHEMA}" SCHEMA_HASH)
file(WRITE "${OUTPUT}.tmp" "(on1x.extension-pin 1 \"Protocols\" \"2\" \"${BINARY_HASH}\" \"${SCHEMA_HASH}\")\n")
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
