import json

import pytest
from dmed import Card, Tool, Transport

VALID_CARD = {
    "dmed_version": "0.2.0",
    "instance_id": "c0ffee01",
    "name": "Smart Coffee Machine",
    "description": "Brews coffee",
    "service_type": "iot_device",
    "transports": [{"type": "http", "url": "http://localhost:3100/mcp", "priority": 1}],
    "auth": {"type": "none"},
    "capabilities": {
        "tools": [
            {"name": "brew_coffee", "description": "Brew coffee"},
            {"name": "get_status", "description": "Get status"},
        ]
    },
}


def test_card_from_dict():
    card = Card.from_dict(VALID_CARD)
    assert card.name == "Smart Coffee Machine"
    assert len(card.tools) == 2


def test_card_from_json():
    card = Card.from_json(json.dumps(VALID_CARD))
    assert card.instance_id == "c0ffee01"


def test_card_from_dict_missing_instance_id_raises():
    bad = {k: v for k, v in VALID_CARD.items() if k != "instance_id"}
    with pytest.raises(KeyError):
        Card.from_dict(bad)


def test_card_from_dict_missing_name_raises():
    bad = {k: v for k, v in VALID_CARD.items() if k != "name"}
    with pytest.raises(KeyError):
        Card.from_dict(bad)


def test_card_tool_names():
    card = Card.from_dict(VALID_CARD)
    names = [t.name for t in card.tools]
    assert "brew_coffee" in names
    assert "get_status" in names


def test_card_tolerates_extra_fields():
    extended = {**VALID_CARD, "future_field": "ignored"}
    card = Card.from_dict(extended)  # should not raise
    assert card.name == "Smart Coffee Machine"


def test_card_parses_transports():
    card = Card.from_dict(VALID_CARD)
    assert len(card.transports) == 1
    assert card.transports[0].type == "http"
    assert card.transports[0].url == "http://localhost:3100/mcp"


def test_card_defaults_when_optional_fields_missing():
    minimal = {"instance_id": "abc123", "name": "Minimal Device"}
    card = Card.from_dict(minimal)
    assert card.service_type == "unknown"
    assert card.description == ""
    assert card.transports == []
    assert card.tools == []
    assert card.auth_type == "none"


def test_card_auth_type_defaults_to_none_when_auth_missing():
    minimal = {"instance_id": "abc123", "name": "Minimal Device"}
    card = Card.from_dict(minimal)
    assert card.auth_type == "none"


class TestCardToDict:
    def test_to_dict_round_trips_core_fields(self):
        card = Card.from_dict(VALID_CARD)
        d = card.to_dict()
        assert d["instance_id"] == "c0ffee01"
        assert d["name"] == "Smart Coffee Machine"
        assert d["service_type"] == "iot_device"

    def test_to_dict_includes_capabilities_tools(self):
        card = Card.from_dict(VALID_CARD)
        d = card.to_dict()
        names = [t["name"] for t in d["capabilities"]["tools"]]
        assert names == ["brew_coffee", "get_status"]

    def test_to_dict_tool_default_input_schema(self):
        card = Card(instance_id="x", name="X", tools=[Tool(name="do_thing", description="does a thing")])
        d = card.to_dict()
        assert d["capabilities"]["tools"][0]["inputSchema"] == {"type": "object", "properties": {}}

    def test_to_dict_omits_empty_description(self):
        card = Card(instance_id="x", name="X")
        d = card.to_dict()
        assert "description" not in d

    def test_to_json_produces_valid_json(self):
        card = Card.from_dict(VALID_CARD)
        parsed = json.loads(card.to_json())
        assert parsed["name"] == "Smart Coffee Machine"

    def test_from_dict_to_dict_round_trip(self):
        card = Card.from_dict(VALID_CARD)
        d = card.to_dict()
        card2 = Card.from_dict(d)
        assert card2.name == card.name
        assert [t.name for t in card2.tools] == [t.name for t in card.tools]


class TestTransport:
    def test_transport_defaults(self):
        t = Transport(type="http")
        assert t.url == ""
        assert t.priority == 10

    def test_transport_explicit_values(self):
        t = Transport(type="ble", url="", priority=1)
        assert t.type == "ble"
        assert t.priority == 1


class TestTool:
    def test_tool_defaults(self):
        t = Tool(name="get_status")
        assert t.description == ""
        assert t.input_schema is None

    def test_tool_with_input_schema(self):
        schema = {"type": "object", "properties": {"temperature": {"type": "number"}}}
        t = Tool(name="set_temperature", description="Set temp", input_schema=schema)
        assert t.input_schema == schema
