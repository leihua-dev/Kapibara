# Contributing to Kapibara

Issues, bug reports and discussion are welcome without any paperwork. Code
contributions have one condition, explained below — please read it before
opening a pull request, because it is easier to agree to up front than to
retrofit.

## Licence and copyright

Kapibara's source is GPLv3 (see `LICENSE`). Contributed code is accepted under
the same licence, **and** the contributor grants the project maintainer the
right to relicense their contribution, including under a commercial licence.

In practice: opening a pull request means you agree that

1. you wrote the contribution, or otherwise have the right to submit it,
2. it may be distributed under GPLv3, and
3. the maintainer may also license it under other terms, including
   proprietary ones, without further approval from you.

You keep the copyright to what you wrote. This is a licence grant, not an
assignment.

### Why

The project may later offer a commercially licensed version. That is only
possible if a single party can license the whole codebase — the moment part of
it is copyright someone else under GPL-only terms, dual licensing needs every
one of those people to individually agree, and in practice that means it never
happens.

This is not hypothetical caution. VCV Rack declines patches without a
contributor agreement for exactly this reason, and has written publicly about
how painful relicensing was when the question came up late. Redis, Elastic,
MongoDB and HashiCorp all had to relicense under pressure and fractured their
communities doing it.

Being explicit on day one is the cheap version of that problem.

## What is *not* covered by the code licence

The GPL applies to the **source code**. It does not apply to content that is a
separate work, in particular:

- preset files (`.mfpreset`)
- wavetables (`.kwt`, imported WAVs)
- samples loaded into the sampler

These carry their own terms. Shipping a preset or wavetable pack does not
oblige anyone to release it under the GPL, and content bundled with a paid
distribution is not made GPL by the fact that the engine playing it is.

## Practical notes

- Keep changes focused; a PR that does one thing is far easier to accept than
  one that does five.
- The realtime rules in `AGENTS.md` are not style preferences. Nothing in the
  audio path may allocate, lock, or free.
- `./scripts/setup-deps.sh` before building — Kapibara needs a patched DPF and
  the patches live in `third_party/dpf-patches/`.
- CI builds every format on Linux, macOS and Windows. Please make sure it is
  green.
