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


## Adding a new algorithm

1. Create a new C++ actor derived from `AEvoCompAlgorithmActor`.
2. Put algorithm-specific settings, results, and action methods on that actor.
3. Add a matching Details customization class for that actor.
4. Register the new actor in the editor module so its Details panel actions appear.
5. Create a Blueprint derived from the new actor and name it `BP_XXX_Main`.
6. Save that Blueprint in `/EvoCompPlugin` and add an editor menu entry for it if you want one-click access.


## Algorithms



<details>

<summary>Genetic Algorithm (GA)</summary>


#### Genetic Algorithm Implementation





- Search space: $x \in [0,1]$, population $P_t = \{x_1^{(t)},\dots,x_N^{(t)}\}$.
- Fitness:
  $$
  \Delta = x - 0.78,
  \quad
  P(x)=e^{-32\Delta^2},
  \quad
  R(x)=\frac{1+\cos(22\Delta)}{2}
  $$
  $$
  f(x)=\min\left(1,\max\left(0,\,P(x)\left(0.78+0.22R(x)\right)\right)\right)
  $$
- Selection (tournament size 2): sample $a,b$ uniformly and pick
  $$
  x_p=
  \begin{cases}
  x_a,& f(x_a)\ge f(x_b)\\
  x_b,& f(x_b)>f(x_a)
  \end{cases}
  $$
- Crossover (probability $p_c$):
  $$
  \alpha\sim U(0.2,0.8),\quad x_c=\alpha x_A+(1-\alpha)x_B
  $$
  otherwise $x_c=x_A$.
- Mutation (probability $p_m$):
  $$
  x_c\leftarrow x_c+\epsilon,\quad \epsilon\sim U(-0.1,0.1),\quad x_c\leftarrow\min(1,\max(0,x_c))
  $$
- Elitism: if enabled, copy best of $P_t$ directly into $P_{t+1}$.
- Running best:
  $$
  F_t=\max\left(F_{t-1},\max_{x\in P_t}f(x)\right)
  $$
- Stability counter with threshold $\tau$:
  $$
  s_t=
  \begin{cases}
  s_{t-1}+1,& F_t\ge\tau\\
  0,& F_t<\tau
  \end{cases}
  $$
- Early stop when both hold: $t\ge G_{\min}$ and $s_t\ge S_{\min}$; else continue to MaxGenerations.

Current defaults:  
PopulationSize 20,  
MaxGenerations 500,   
MutationRate 0.08,   
CrossoverRate 0.80,   
FitnessThreshold 0.965,   
MinGenerationsBeforeStop 30,   
RequiredStableGenerations 8,   
bEnableElitism true.



</details>

