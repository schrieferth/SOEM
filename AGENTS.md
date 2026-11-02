# AI Development Rules

These rules apply to the Testknecht product repositories — `pyTestknecht`, `testknecht`,
`piTestknecht-signalzwerg`, `wpTestknecht`, `SOEM`. They form **one product
workspace**: when working in any of them, treat the others as relevant context unless the user
explicitly limits the task to one repository.

## Read this first: the rules are routed, not recited

This file used to be 1027 lines, duplicated identically into five repositories and loaded in
full before every task. Writing a procedure meant reading 166 lines of instrument-driver
contract and 157 lines of HIL-teststand architecture that could not possibly apply. Measured
relevance was 6–35 %, and the rest is not harmless: it pushes the rule that *does* apply out of
attention.

The full rule text is unchanged — it lives in `pyTestknecht/.ai/rules/`, split by subject. What
changed is that you now **ask which parts apply**:

```bash
cd pyTestknecht
uv run python tools/ai_rules.py <the files you are about to touch>
uv run python tools/ai_rules.py --changed          # or: whatever git says is dirty
uv run python tools/ai_rules.py --exemplar sweep   # which shipped example to copy
```

It answers with four things: the **rules** to read, the **facts** to consult, the **example** to
copy, and the **command that proves the work**. `.ai/rules/core.md` is always among them.

`pyTestknecht/.ai/rules.yaml` is the router itself, and `uv run python tools/ai_rules.py --check`
fails the build if it ever names a rule file, fact file, test or example that does not exist —
because the failure this replaced was an instruction that could not be followed.

### The facts are data, not prose

| file | what it answers |
| --- | --- |
| `.ai/facts/drivers.yaml` | *generated* — every registered model's maturity, evidence-based contract level (never inferred from `PROVIDES` alone), transport certificate and parameter map. Which drivers may be copied, and which must not. |
| `.ai/facts/examples.yaml` | the shipped example catalog, with `status` and `exemplar_for`. What to copy for a given topic. |
| `.ai/facts/incidents.yaml` | *curated* — the incidents the rules were written from: symptom, root cause, rule, and the guard that makes it impossible to repeat. |

Regenerate the driver facts with `uv run python tools/generate_driver_facts.py`; `--check` is a
CI gate against staleness.

## The non-negotiables

These hold for every task, in every repository, and are repeated here because they prevent
damage rather than merely guide style. The reasoning behind each one is in `.ai/rules/core.md`
and, with the incident it came from, in `.ai/facts/incidents.yaml`.

1. **No test may be silently green.** A procedure that reports a failure must be recorded as one.
   `decision_passed()` honours `passed` → `verdict` → `checks` → `decision`; only a decision that
   says *nothing* about its outcome defaults to success, and that case is warned about. A green
   report over a red bus is the worst artifact this product can produce.
2. **Never hide a framework gap behind procedure code.** When the declarative path cannot do what
   is needed, name the gap and let the user decide. The default is the **clean path**: close the
   gap, extend the contract, update the affected examples, tests and docs in the same session.
3. **A procedure is one operating point.** Sweeps, grids and repetitions belong in the plan
   `matrix`; cross-point evidence is a summary fixture (`summary_row` + `group_by` +
   `summarize`). A grid loop inside a procedure is a defect, not a style choice.
4. **The workbench owns real-vs-dummy selection.** Plans and procedures reference one stable
   alias; the workspace instruments file gives it a real `model` plus `dummy_policy`.
5. **The parameter map is enforced, not merely declared.** It is the single source the real
   driver, the dummy and the simulation are built from. A map entry the driver does not honour is
   a contract defect, not a documentation defect.
6. **Verify against the configuration the user asked for.** Never present a green dummy run as
   evidence for a hardware change. State the backend explicitly when reporting results.
7. **Documentation and tests are part of the implementation**, not follow-up polish. Every
   behavior change carries matching doc updates and a test that would fail without it — or an
   explicit statement of why a test is not practical.
8. **Do not commit or push unless the user explicitly asks.** Preserve unrelated user changes in
   all product repositories. On an explicit request, `.ai/rules/commit.md` has the full process
   (preflight, version bump, changelog, repository order).
9. **Cross-repo rule changes travel together.** An AI rule, workflow, styleguide or convention
   added in one repository is applied to the others in the same session. Run
   `python3 .github/scripts/check_ai_sync.py` afterwards.
10. **Run the smallest justified test selection, not a suite by habit.** Select the directly
    affected test nodes and state what risk each one covers. Run a broader file or suite only when
    cross-cutting behavior, collection/fixture effects, packaging, or an explicit release gate
    makes it necessary; explain that necessity before starting it. Reuse unchanged evidence and
    never rerun unrelated tests merely for commit/push.
11. **Every AI-started test run is visible and produces a report.** Start pytest only through
    `pyTestknecht/tools/ide_pytest.py`; it mirrors start, progress, diagnostics and completion into
    the `Testknecht Test Explorer` output channel and writes a durable JUnit report below
    `pyTestknecht/.testknecht/ai-test-reports/`. Testknecht plan runs use the permanent operator
    server on port `1111`, remain observable on `/run_view`, and produce the canonical Testknecht
    report. Do not start or advertise another user-facing test/live/report port. Add equivalent
    progress and report support before using another test runner; non-test read-only guards are
    exempt.
12. **Root cause before remedy; no quick fixes.** Establish the evidence chain from symptom to
    violated contract and owning defect before changing behavior. Fix the owning boundary with the
    clean target architecture and a regression test that fails for the original cause. Do not
    substitute retries, sleeps, swallowed errors, disabled checks, compatibility shims or local
    workarounds unless they are the technically correct long-term design and are documented as
    such. Use current primary sources when external technology affects the design, and state any
    residual uncertainty or risk explicitly.

## Rule sources beyond this file

The router names the ones that match your task. Beyond them, read as needed:
`pyTestknecht/docs/**/*.rst`, `testknecht/docs/**` (contracts, ADRs, internals),
`piTestknecht-signalzwerg/docs/**`,
`wpTestknecht/docs/**`, `pyTestknecht/_local_workfolder/*.md`, and the `.github/` skills,
prompts, instructions and knowledge files.

Use targeted reads. Identify the documents that match the touched area, then follow their
contracts — do not load everything blindly. That habit is the whole reason this file is short.

## QCoDeS is part of the product

QCoDeS is the **driver layer** (ADR-023/024/036/038) and an optional persistence sink (ADR-037),
so its documentation is a rule source. Before designing measurement, instrument or dataset
behavior, check what QCoDeS already does — read it from the installed version
(`python -c "import qcodes; help(qcodes.dataset.dond)"`), not from memory. It already owns
`Parameter` (a measured value with its own `label`/`unit` — never re-declare these as `Column`),
`Station` (with `snapshot()`), `Measurement`/`DataSaver`/`load_by_id`, and `dond` with sweep
objects. What QCoDeS deliberately does **not** do, and Testknecht must: set the operating point,
guarantee the bench is safe afterwards, reach a verdict, produce the report, cover non-QCoDeS
devices. Details in `.ai/rules/driver.md`.
