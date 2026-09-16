"""Pure admission/eviction planner for a future isolated native cache.

No filesystem writes or Habitat cache eviction. Sizes must include all entry
files; callers must hold a cache-wide lock through eviction and admission.
"""


def plan_admission(entries, incoming_bytes, max_bytes, max_entries):
    """Return oldest-first keys to evict, or None for a rejected admission.

    entries: (key, size_bytes, last_access) tuples in this cache's namespace.
    Incoming staging bytes must also fit: victims are removed before writing.
    """
    if max_bytes <= 0 or max_entries <= 0 or incoming_bytes < 0:
        raise ValueError("Invalid cache budget")
    if any(size < 0 for _, size, _ in entries):
        raise ValueError("Invalid entry size")
    if len({key for key, _, _ in entries}) != len(entries):
        raise ValueError("Duplicate cache keys")
    if incoming_bytes > max_bytes:
        return None
    remaining = sum(size for _, size, _ in entries)
    count = len(entries)
    evicted = []
    for key, size, _ in sorted(entries, key=lambda item: (item[2], item[0])):
        if remaining + incoming_bytes <= max_bytes and count + 1 <= max_entries:
            break
        evicted.append(key)
        remaining -= size
        count -= 1
    return evicted
