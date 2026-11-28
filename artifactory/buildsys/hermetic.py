def verify(action, allowed_env=()):
    """Check that an action only declares environment names in its allowlist."""
    env = set(getattr(action, "env", {}) or {})
    allowed = set(allowed_env or ())
    return env.issubset(allowed)
