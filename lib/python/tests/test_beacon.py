import pytest
from dmed import Beacon, Flag, ServiceType, crc16, crc32, instance_id_from


def test_crc16_is_deterministic():
    assert crc16(b"dmed") == crc16(b"dmed")


def test_crc16_differs_for_different_input():
    assert crc16(b"dmed") != crc16(b"other")


def test_crc32_is_deterministic():
    assert crc32(b"dmed") == crc32(b"dmed")


def test_crc32_differs_for_different_input():
    assert crc32(b"dmed") != crc32(b"other")


def test_crc32_empty_input():
    assert crc32(b"") == 0


def test_instance_id_from_returns_8_hex_chars():
    iid = instance_id_from("Smart Coffee Machine")
    assert len(iid) == 8
    int(iid, 16)  # does not raise


def test_instance_id_from_is_deterministic():
    assert instance_id_from("Smart Coffee Machine") == instance_id_from("Smart Coffee Machine")


def test_instance_id_from_differs_by_name():
    assert instance_id_from("Device A") != instance_id_from("Device B")


class TestBeaconEncodeDecode:
    def test_roundtrip_minimal(self):
        b = Beacon(version=1, flags=Flag.AUTH, service_type=ServiceType.IOT_DEVICE, instance_id=0xC0FFEE01)
        decoded = Beacon.decode(b.encode())
        assert decoded.version == 1
        assert decoded.flags == Flag.AUTH
        assert decoded.service_type == ServiceType.IOT_DEVICE
        assert decoded.instance_id == 0xC0FFEE01

    def test_roundtrip_with_tx_power(self):
        b = Beacon(instance_id=1, tx_power=-40)
        decoded = Beacon.decode(b.encode())
        assert decoded.tx_power == -40

    def test_roundtrip_with_tx_power_and_name_hash(self):
        b = Beacon(instance_id=1, tx_power=-40, name_hash=0xBEEF)
        decoded = Beacon.decode(b.encode())
        assert decoded.tx_power == -40
        assert decoded.name_hash == 0xBEEF

    def test_encode_without_optional_fields_omits_them(self):
        b = Beacon(instance_id=1)
        assert len(b.encode()) == 6

    def test_decode_too_short_raises_value_error(self):
        with pytest.raises(ValueError):
            Beacon.decode(b"\x00\x00")

    def test_instance_id_hex_property(self):
        b = Beacon(instance_id=0xC0FFEE01)
        assert b.instance_id_hex == "c0ffee01"

    def test_to_mdns_txt_contains_core_fields(self):
        b = Beacon(version=1, flags=Flag.MULTI, service_type=ServiceType.IOT_DEVICE, instance_id=0xC0FFEE01)
        txt = b.to_mdns_txt(name="Coffee Machine")
        assert txt["id"] == "c0ffee01"
        assert txt["nm"] == "Coffee Machine"
        assert txt["path"] == "/mcp"
        assert txt["card"] == "/dmed/card"

    def test_to_mdns_txt_without_name_omits_nm(self):
        b = Beacon(instance_id=1)
        txt = b.to_mdns_txt()
        assert "nm" not in txt
