# TEMPLATE_PHILOSOPHY.md
### Principles behind the JUCE Plugin Template

This document explains **why** the template is built the way it is.
It is short by design. If you understand this page, you will not fight the architecture.

---

> 🔗 Related documents:  
> - [README](README.md)  
> - [Architecture Overview](docs/ARCHITECTURE.md)  
> - [New Plugin Checklist](NEW_PLUGIN_CHECKLIST.md)


## 1. One source of truth

**Parameters own truth.**

- DSP reads from Parameters
- UI writes intent to Parameters
- Hardware writes intent to Parameters
- Hardware output mirrors Parameters

No other layer owns state.

If you ever feel the urge to “just update the UI value directly” or
“just tweak DSP from hardware” — stop. That is how bugs are born.

---

## 2. Normalized control space

Hardware, automation, and modulation all work in **normalized space (0..1)**.

Why:
- Encoders are inherently relative
- Hardware does not understand dB, Hz, or gain
- Normalized values scale cleanly
- Parameter ranges can change without breaking hardware

Mapping to native units happens **once**, inside the binding.

---

## 3. Explicit routing beats magic

This template avoids:
- Implicit observers
- Hidden callbacks
- “Framework magic”

Every control path is explicit:
```
Input → Binding → Parameters → Output
```

This makes the system:
- Debuggable
- Predictable
- Easy to extend

If you can’t trace a value on paper, the design is wrong.

---

## 4. Focus is a first-class concept

Focus is not a UI detail.
Focus defines **intent**.

Focus decides:
- Which parameter encoders affect
- Which LEDs light up
- What hardware considers “active”

That’s why focus:
- Has an ID
- Is persisted
- Is visible in UI and hardware

---

## 5. Hardware mirrors state — it never drives it

Hardware output reflects current state.
It does not decide state.

This separation guarantees:
- No feedback loops
- No desync
- No race conditions

Hardware is a view, not a brain.

---

## 6. Fewer abstractions, but the right ones

This template uses:
- A small platform layer (`ui_core`)
- Explicit adapters for hardware

It avoids:
- Over-generalized frameworks
- Deep inheritance trees
- “One class to rule them all”

Abstractions exist only when they remove real duplication.

---

## 7. Scale before cleverness

The architecture favors:
- Boring clarity
- Repeatable patterns
- Long-term scalability

It deliberately sacrifices:
- Short-term speed
- Fancy tricks
- “Just this once” hacks

You will thank yourself on plugin #5.

---

## 8. Template rule of thumb

Before adding code, ask:

> “Does this introduce a second source of truth?”

If yes — don’t do it.

---

## 9. Final note

This template is not about JUCE.
JUCE is just the rendering and audio backend.

This template is about:
- Control flow
- Ownership
- Hardware readiness
- Predictability

Treat it like a platform, not a project.
