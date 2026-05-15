# Contributing to DMED

Thanks for your interest in contributing to the Dynamic MCP Endpoint Discovery Protocol!

## How to Contribute

### Ideas & Feedback

- Open an issue to discuss protocol changes, new features, or use cases
- Share examples of things you'd like to make discoverable

### Code Contributions

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-idea`)
3. Make your changes
4. Submit a pull request

### Adding Examples

We welcome new examples of DMED-enabled MCP endpoints. Each example should include:

- `dmed-manifest.json` — endpoint manifest
- `server.js` (or equivalent) — working MCP endpoint with DMED broadcasting
- `package.json` — dependencies
- `README.md` — setup and usage instructions

### Protocol Spec Changes

For changes to the DMED protocol itself, please open an issue first to discuss the proposal before submitting a PR.

## Code Style

- Use ES modules (`import`/`export`)
- Keep implementations minimal and readable
- Include comments for non-obvious logic

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
