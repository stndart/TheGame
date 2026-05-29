Name the file as `<YYYY-MM-DD>-<order>-<short title>`
`order` is a two-digit number to semantically order the logs.

# Investigation: <short title>

**Date:** YYYY-MM-DD  
**Run / refs:** `logs/runs/<run_id>/` (if applicable), IDA addresses, PR/issue links

---

## Problem statement

What was wrong or unknown? What triggered this investigation?

---

## Research

What did you read, decompile, grep, or trace? Key findings only (details go in the long journal).

---

## Hypothesis

What you believed was going on before changing code. List alternatives if relevant.

---

## Implementation

What you changed (files, hooks, symbols). How it maps to the hypothesis.

---

## Diagnostics

How you verified: `just` / `ctl` session, stages reached, log lines, hook hits, comparisons to original binary or engine source ideas.

---

## Conclusion

Outcome: fixed, partial, WIP, dead end. What to do next. Lessons or caveats.

---

## Long journal

Every brief entry above must have a **mirrored long-form** counterpart:

- Brief: `journals/<YYYY-MM-DD>-<order>-<slug>.md` (this file, from this template)
- Long: `journals/long/<YYYY-MM-DD>-<order>-<slug>.md` (same basename; `order` is two digits, see filename line above)

The long journal holds full decompilation notes, command transcripts, screenshots paths, dead ends, and raw observations. Structure is optional; completeness matters.

`journals/` is gitignored except this template - do not commit investigation files unless the team explicitly asks.
