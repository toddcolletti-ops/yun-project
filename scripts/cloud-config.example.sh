# Arduino Cloud configuration
# Copy this file to cloud-config.sh and fill in your credentials.
# cloud-config.sh is gitignored and will not be committed.

# API credentials (from Arduino Cloud profile > API Keys)
API_CLIENT_ID="your_api_client_id"
API_CLIENT_SECRET="your_api_client_secret"

# Device
DEVICE_ID="your_device_uuid"
DEVICE_SECRET="your_device_secret"

# Thing
THING_ID="your_thing_uuid"

# Property UUIDs (find these via: ./scripts/cloud-status.sh)
PROP_TEMPERATURE="your_temperature_property_uuid"
PROP_HUMIDITY="your_humidity_property_uuid"
PROP_LIGHT="your_light_property_uuid"
PROP_ANALOG_RAW="your_analog_raw_property_uuid"

# Yun network
YUN_HOST="192.168.11.36"
