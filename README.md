# EvoCompPlugin

Unreal Engine 5 C++ plugin with Evolutionary Computation algorithms.

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


## Genetic Algorithm Implementation

This section describes the current GA in mathematical form.

### Search space

- Each individual is a single real gene $x \in [0,1]$.
- Population at generation $t$ is:
  $$P_t = \{x_1^{(t)}, x_2^{(t)}, \dots, x_N^{(t)}\}$$

### Fitness function

The objective combines a narrow global peak near $0.78$ and a local ripple term.

$$
\Delta = x - 0.78
$$

$$
	ext{Peak}(x) = e^{-32\Delta^2}
$$

$$
	ext{Ripple}(x) = \frac{1 + \cos(22\Delta)}{2}
$$

$$
f(x) = \operatorname{clamp}\left(\text{Peak}(x) \cdot \left(0.78 + 0.22\,\text{Ripple}(x)\right),\ 0,\ 1\right)
$$

### Selection

Tournament selection with tournament size 2:

1. Sample two indices $a,b$ uniformly from population indices.
2. Select parent gene as
   $$
   x_{\text{parent}} =
   \begin{cases}
   x_a & f(x_a) \ge f(x_b) \\
   x_b & \text{otherwise}
   \end{cases}
   $$

### Crossover

With probability $p_c$ (`CrossoverRate`), offspring is blended from two parents:

$$
\alpha \sim U(0.2, 0.8), \quad
x_{\text{child}} = \alpha x_A + (1-\alpha)x_B
$$

Otherwise, offspring starts as $x_A$.

### Mutation

With probability $p_m$ (`MutationRate`), apply additive perturbation:

$$
x_{\text{child}} \leftarrow x_{\text{child}} + \epsilon, \quad \epsilon \sim U(-0.1, 0.1)
$$

Then clamp to domain:

$$
x_{\text{child}} \leftarrow \operatorname{clamp}(x_{\text{child}}, 0, 1)
$$

### Elitism

If `bEnableElitism = true`, copy the best individual from $P_t$ directly into $P_{t+1}$ before filling remaining slots.

### Stopping criteria

At each generation, track global best fitness:

$$
F_t = \max(F_{t-1}, \max_{x \in P_t} f(x))
$$

Track consecutive generations at or above threshold $\tau$ (`FitnessThreshold`):

$$
s_t =
\begin{cases}
s_{t-1}+1 & F_t \ge \tau \\
0 & F_t < \tau
\end{cases}
$$

Stop early only when both are true:

- $t \ge G_{\min}$ (`MinGenerationsBeforeStop`)
- $s_t \ge S_{\min}$ (`RequiredStableGenerations`)

Otherwise continue until `MaxGenerations`.

### Current default configuration

- PopulationSize: 20
- MaxGenerations: 500
- MutationRate: 0.08
- CrossoverRate: 0.80
- FitnessThreshold: 0.965
- MinGenerationsBeforeStop: 30
- RequiredStableGenerations: 8
- bEnableElitism: true