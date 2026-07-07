import pytest

requests = pytest.importorskip("requests")

from dmed import DMEDClient


class FakeResponse:
    def __init__(self, json_data, status_code=200):
        self._json = json_data
        self.status_code = status_code

    def json(self):
        return self._json

    def raise_for_status(self):
        if self.status_code >= 400:
            raise requests.HTTPError(f"{self.status_code} error")


CARD_JSON = {
    "instance_id": "c0ffee01",
    "name": "Smart Coffee Machine",
    "service_type": "iot_device",
    "capabilities": {"tools": [{"name": "brew_coffee", "description": "Brew coffee"}]},
}


def test_client_card_is_none_before_connect():
    client = DMEDClient("http://localhost:3100")
    assert client.card is None


def test_client_strips_trailing_slash_from_base_url():
    client = DMEDClient("http://localhost:3100/")
    assert client.base_url == "http://localhost:3100"


def test_client_sets_authorization_header_when_token_given():
    client = DMEDClient("http://localhost:3100", auth_token="secret")
    assert client._headers["Authorization"] == "Bearer secret"


def test_client_omits_authorization_header_without_token():
    client = DMEDClient("http://localhost:3100")
    assert "Authorization" not in client._headers


def test_connect_fetches_and_caches_card(monkeypatch):
    calls = {}

    def fake_get(url, headers=None, timeout=None):
        calls["url"] = url
        return FakeResponse(CARD_JSON)

    monkeypatch.setattr(requests, "get", fake_get)

    client = DMEDClient("http://localhost:3100")
    card = client.connect()

    assert calls["url"] == "http://localhost:3100/dmed/card"
    assert card.name == "Smart Coffee Machine"
    assert client.card is card


def test_list_actions_returns_actions_array(monkeypatch):
    def fake_get(url, headers=None, timeout=None):
        return FakeResponse({"actions": [{"name": "brew_coffee", "description": "Brew coffee"}]})

    monkeypatch.setattr(requests, "get", fake_get)

    client = DMEDClient("http://localhost:3100")
    actions = client.list_actions()

    assert len(actions) == 1
    assert actions[0]["name"] == "brew_coffee"


def test_send_action_posts_action_and_params(monkeypatch):
    captured = {}

    def fake_post(url, json=None, headers=None, timeout=None):
        captured["url"] = url
        captured["json"] = json
        return FakeResponse({"status": "ok", "action": "brew_coffee", "result": {"text": "done"}})

    monkeypatch.setattr(requests, "post", fake_post)

    client = DMEDClient("http://localhost:3100")
    resp = client.send_action("brew_coffee", {"drink_type": "latte"})

    assert captured["url"] == "http://localhost:3100/dmed/action"
    assert captured["json"] == {"action": "brew_coffee", "params": {"drink_type": "latte"}}
    assert resp["status"] == "ok"


def test_send_action_defaults_params_to_empty_dict(monkeypatch):
    captured = {}

    def fake_post(url, json=None, headers=None, timeout=None):
        captured["json"] = json
        return FakeResponse({"status": "ok"})

    monkeypatch.setattr(requests, "post", fake_post)

    client = DMEDClient("http://localhost:3100")
    client.send_action("get_status")

    assert captured["json"] == {"action": "get_status", "params": {}}


def test_call_tool_sends_json_rpc_payload(monkeypatch):
    captured = {}

    def fake_post(url, json=None, headers=None, timeout=None):
        captured["url"] = url
        captured["json"] = json
        return FakeResponse({
            "jsonrpc": "2.0",
            "id": 1,
            "result": {"content": [{"type": "text", "text": "done"}]},
        })

    monkeypatch.setattr(requests, "post", fake_post)

    client = DMEDClient("http://localhost:3100")
    result = client.call_tool("brew_coffee", {"drink_type": "latte"})

    assert captured["url"] == "http://localhost:3100/mcp"
    assert captured["json"]["method"] == "tools/call"
    assert captured["json"]["params"] == {"name": "brew_coffee", "arguments": {"drink_type": "latte"}}
    assert result["content"][0]["text"] == "done"


def test_connect_raises_on_http_error(monkeypatch):
    def fake_get(url, headers=None, timeout=None):
        return FakeResponse({}, status_code=500)

    monkeypatch.setattr(requests, "get", fake_get)

    client = DMEDClient("http://localhost:3100")
    with pytest.raises(requests.HTTPError):
        client.connect()
