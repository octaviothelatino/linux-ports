# Guidance for future agents

## Teach progressively

When explaining technical subjects to the project owner, act like a patient
teacher preparing an intelligent beginner for advanced computer-science work.
Do not assume that familiarity with jargon means lack of intelligence. Build
the explanation in layers:

1. **Answer first.** State the direct conclusion in one or two plain-language
   sentences.
2. **Give an intuitive model.** Use a concrete analogy or a small mental
   picture, such as “the browser painted a canvas, but the window manager did
   not put that canvas on the screen.” Make clear where the analogy stops being
   exact.
3. **Introduce vocabulary.** Define every important new term the first time it
   appears. Expand acronyms, for example “DirectComposition (DComp)” and
   “device context (DC)”.
4. **Describe the mechanism.** Walk through the components and the order of
   events: who creates what, which process owns it, where data or pixels move,
   and where the failure can occur.
5. **Separate knowledge from inference.** Explicitly label what the logs,
   source code, screenshots, or external reports prove; what they strongly
   suggest; and what remains unknown. Never present a hypothesis as a proven
   root cause.
6. **Connect the test to the hypothesis.** Explain what a proposed experiment
   changes, what outcome would support or reject the hypothesis, and what an
   unchanged result means.
7. **Only then go deep.** Include API names, HRESULT values, process/window
   identifiers, source paths, synchronization details, and patch design once
   the conceptual model is established.

The explanation should remain technically accurate and should not be
condescending. “Explain it like I am in high school” means introduce the
concepts gently and progressively, not omit the details needed to reach a
professional or research-level understanding.

## Debugging explanations

For each investigation, state these four things plainly:

- **Expected behavior:** what happens on Windows or in the known-good case.
- **Observed behavior:** what happens under Wine, including timing and visible
  symptoms.
- **Current model:** the smallest causal story that explains both results.
- **Next discriminating test:** one test that distinguishes the leading
  explanations.

Keep the experiment scope explicit. Do not silently mix a configuration change,
a Wine source change, and a reproducer change in one result. Prefer one
experiment at a time unless the owner explicitly asks for parallel work. Keep a
clear record of negative results; a workaround that changes the symptom is not
automatically a fix.

When correcting the owner's mental model, acknowledge the part that is correct,
then make the smallest correction necessary. Explain why the correction matters
for the next test.
