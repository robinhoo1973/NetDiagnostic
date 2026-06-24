#!/bin/bash
# ─────────────────────────────────────────────────────────────────────
# asc-app.sh — App Store Connect API helpers
# ─────────────────────────────────────────────────────────────────────
# Usage:
#   source scripts/ios/asc-app.sh
#   asc_jwt       API_KEY_ID ISSUER_ID PRIVATE_KEY_PATH   → prints JWT
#   asc_app_find  JWT BUNDLE_ID                          → prints app ID or ""
#   asc_app_create JWT APP_NAME BUNDLE_ID SKU LOCALE      → prints app ID
#   asc_app_ensure JWT APP_NAME BUNDLE_ID SKU LOCALE      → idempotent ensure
# ─────────────────────────────────────────────────────────────────────

set -euo pipefail

ASC_API="https://api.appstoreconnect.apple.com/v1"

# ─────────────────────────────────────────────────────────────────────
# Generate JWT for App Store Connect API (valid 20 min)
# ─────────────────────────────────────────────────────────────────────
asc_jwt() {
    local key_id="$1"
    local issuer_id="$2"
    local key_path="$3"

    local header
    header=$(printf '{"alg":"ES256","kid":"%s","typ":"JWT"}' "$key_id" | base64 | tr -d '=' | tr '/+' '_-')
    local now
    now=$(date +%s)
    local exp=$((now + 1200))
    local payload
    payload=$(printf '{"iss":"%s","iat":%d,"exp":%d,"aud":"appstoreconnect-v1"}' \
        "$issuer_id" "$now" "$exp" | base64 | tr -d '=' | tr '/+' '_-')
    local signature
    signature=$(printf '%s.%s' "$header" "$payload" | openssl dgst -sha256 -sign "$key_path" | base64 | tr -d '=' | tr '/+' '_-')
    echo "${header}.${payload}.${signature}"
}

# ─────────────────────────────────────────────────────────────────────
# Find an app by bundle ID → prints app id or empty string
# ─────────────────────────────────────────────────────────────────────
asc_app_find() {
    local jwt="$1"
    local bundle_id="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/apps?filter[bundleId]=${bundle_id}" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    # If curl failed entirely
    if [ $? -ne 0 ]; then
        echo "ERROR: curl request failed" >&2
        return 1
    fi

    local app_id
    app_id=$(echo "$resp" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',[{}])[0].get('id',''))" 2>/dev/null || echo "")

    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# Create an app → prints app id
# ─────────────────────────────────────────────────────────────────────
asc_app_create() {
    local jwt="$1"
    local app_name="$2"
    local bundle_id="$3"
    local sku="$4"
    local locale="${5:-en-US}"

    local payload
    payload=$(cat <<ENDJSON
{
  "data": {
    "type": "apps",
    "attributes": {
      "name": "${app_name}",
      "bundleId": "${bundle_id}",
      "primaryLocale": "${locale}",
      "sku": "${sku}"
    }
  }
}
ENDJSON
)

    local resp
    resp=$(curl -s -w "\n%{http_code}" -X POST "${ASC_API}/apps" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$payload")

    local http_code
    http_code=$(echo "$resp" | tail -1)
    local body
    body=$(echo "$resp" | sed '$d')

    if [ "$http_code" != "201" ] && [ "$http_code" != "200" ]; then
        echo "ERROR: failed to create app (HTTP ${http_code}): ${body}" >&2
        return 1
    fi

    local app_id
    app_id=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['id'])")
    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# Ensure app exists — find or create — prints app id
# ─────────────────────────────────────────────────────────────────────
asc_app_ensure() {
    local jwt="$1"
    local app_name="$2"
    local bundle_id="$3"
    local sku="$4"
    local locale="${5:-en-US}"

    local app_id
    app_id=$(asc_app_find "$jwt" "$bundle_id")

    if [ -n "$app_id" ]; then
        echo "App already exists: id=${app_id}" >&2
        echo "$app_id"
        return 0
    fi

    echo "App not found, creating: name=${app_name} bundle=${bundle_id}..." >&2
    app_id=$(asc_app_create "$jwt" "$app_name" "$bundle_id" "$sku" "$locale")
    echo "Created app: id=${app_id}" >&2
    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# When executed directly (not sourced), run the requested function
# ─────────────────────────────────────────────────────────────────────
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    CMD="${1:-}"
    shift 2>/dev/null || true
    case "$CMD" in
        jwt)          asc_jwt "$@" ;;
        find)         asc_app_find "$@" ;;
        create)       asc_app_create "$@" ;;
        ensure)       asc_app_ensure "$@" ;;
        *)
            echo "Usage: $0 {jwt|find|create|ensure} [args...]" >&2
            exit 1
            ;;
    esac
fi
