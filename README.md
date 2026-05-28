# EvoCompPlugin

A UE 5.7 plugin that provides a Details-panel-driven Genetic Algorithm workflow.

## Overview

This plugin includes:

- A shared evolutionary base actor: `AEvoCompAlgorithmActor`
- A runtime GA actor class: `AEvoCompGeneticAlgorithm`
- A plugin-level Blueprint utility node: `Execute Genetic Algorithm`
- An editor menu shortcut:
  - `Window -> Genetic Algorithm -> Open Genetic Algorithm Main Blueprint`
- A custom Details panel for the GA actor classes with dedicated action buttons:
  - `Run Genetic Algorithm`
  - `Step One Generation`
  - `Reset Population`

## Current recommended workflow

1. Build the project/plugin.
2. In Unreal Editor, create a Blueprint derived from `AEvoCompGeneticAlgorithm` and name it `BP_GA_Main`.
3. Save `BP_GA_Main` in plugin content path `/EvoCompPlugin`.
4. Open it from the editor menu:
  - `Window -> Genetic Algorithm -> Open Genetic Algorithm Main Blueprint`
5. Use the Details panel categories:
  - `GA | Configuration`
  - `GA | Results`
  - `GA | Actions`

For future algorithms, add a new concrete actor derived from `AEvoCompAlgorithmActor`, then create a matching blueprint such as `BP_XXX_Main` with its own defaults and menu entry.

## Adding a new algorithm

1. Create a new C++ actor derived from `AEvoCompAlgorithmActor`.
2. Put algorithm-specific settings, results, and action methods on that actor.
3. Add a matching Details customization class for that actor.
4. Register the new actor in the editor module so its Details panel actions appear.
5. Create a Blueprint derived from the new actor and name it `BP_XXX_Main`.
6. Save that Blueprint in `/EvoCompPlugin` and add an editor menu entry for it if you want one-click access.

## Configuration fields

`GA | Configuration`:

- `PopulationSize`
- `MaxGenerations`
- `MutationRate`
- `CrossoverRate`
- `FitnessThreshold`
- `bEnableElitism`

`GA | Results` (read-only):

- `CurrentGeneration`
- `BestFitness`

## Action behavior

`Run Genetic Algorithm`:

- Resets internal state
- Initializes a population
- Executes generations until `MaxGenerations` or `FitnessThreshold` is reached
- Prints generation-by-generation logs

`Step One Generation`:

- Runs exactly one generation from current state
- Prints start/end generation logs and threshold status

`Reset Population`:

- Clears GA state and counters
- Prints a reset log

## Implemented GA model (current)

The actor runs a simple, deterministic-structure GA with stochastic evolution:

- Gene representation: single float in `[0, 1]`
- Fitness: parabola centered near `0.8` (`1 - (x - 0.8)^2`, clamped)
- Parent selection: tournament selection
- Crossover: weighted blend between two parents
- Mutation: small additive perturbation with clamping
- Elitism: optional carry-over of best individual

This is a practical baseline and suitable for editor experimentation. It can be extended to problem-specific genome and fitness functions later.

