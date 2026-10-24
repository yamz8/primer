---
name: primer
description: Design and check PCR primers with the primer command. Use for Tm of an oligo (SantaLucia 1998 nearest-neighbor with Na+/Mg2+/dNTP correction), checking primers for GC%, hairpins, self- and cross-dimers, and binding sites, or designing primer pairs flanking a target region of a FASTA/GenBank template.
---

# Primer

`primer` predicts; it does not replace a lab test. Say so when you report results.

## Commands

- `primer tm <seq> [--na 50] [--mg 1.5] [--dntp 0.6] [--conc 50] [--json]`
  prints the melting temperature in °C (salt in mM, primer in nM).
- `primer check <forward> [<reverse>] [--template file] [--json] [--report out.pdf|out.svg]`
  checks one primer or a pair. Exits 1 when any check fails. With a template
  it also finds binding sites on both strands and the product size.
- `primer design <template> --region start..end [--json] [--csv out.csv|-] [--report out.pdf] [--save out.primer]`
  designs ranked pairs whose product spans the target. Exits 1 when no pair passes.

Settings shared by check and design: `--length 18-25 --length-opt 20 --tm 57-63
--tm-opt 60 --tm-diff 2 --gc 40-60 --clamp 1 --product 100-1000 --pairs 50`.
Positions are 1-based and inclusive.

## Reading the JSON

Each primer has `tm_c`, `gc_percent`, `status` (`pass`, `warn`, `fail`), and
`checks`, a list of `{check, status, message}`. Structures (`hairpin`,
`self_dimer`, `cross_dimer`) carry `dg_kcal_per_mol` at 37 °C,
`three_prime` (the 3' end is paired, so the polymerase can extend it), and a
`drawing` to show the user verbatim in a code block. With a template,
`binding_sites` lists every site (`strand`, `start`, `end`, `mismatches`); more
than one makes the primer non-unique.

## Opening the app

`primer open file.primer` (or a FASTA/GenBank file) opens the window.
