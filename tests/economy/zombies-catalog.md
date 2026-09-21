# Zombies orders and contracts

The feat/39a approach applied to Zombies: larger pools, existing offer and active limits, native identities and event rules. Targets for orders and all AC prices/rewards are local balance choices. No profile reset is required.

## Daily orders

250 AC each; six offers per day from 20 options.

| Order | Objective |
|---|---|
| Dead on Target | Kill 25 zombies with headshots |
| Close Quarters | Kill 50 zombies with shotguns |
| Sidearm Specialist | Kill 50 zombies with pistols |
| Suppressive Fire | Kill 75 zombies with LMGs |
| Improved Firepower | Kill 100 zombies with upgraded weapons |
| Explosive Results | Kill 40 zombies with explosives |
| Pest Control | Kill 15 Pests with melee attacks |
| Deadeye | Kill 20 zombies with sniper headshots |
| Let the Trap Work | Kill 50 zombies with traps |
| Sidearm Overdrive | Kill 60 zombies with upgraded pistols |
| Bomb Disposal | Kill 5 Bombers without detonating their bombs |
| Knife Work | Kill 15 zombies with throwing knives |
| Surprise Package | Kill 40 zombies with Jack-in-the-Boxes |
| Heavy Handed | Kill 2 Wustlings using only melee attacks |
| Return to Sender | Kill 15 zombies with Bomber bombs |
| Running on Empty | Kill 3 zombies while out of ammo |
| Cutting Crew | Kill 25 zombies with the Ripsaw heavy attack on The Darkest Shore |
| Chain Reaction | Kill a Bomber with a Bomber bomb |
| Propeller Cleanup | Kill 30 zombies with the Sub Pen trap on The Darkest Shore |
| Spine Collector | Kill 2 Meuchlers with the charged Ripsaw attack on The Darkest Shore |

## Weekly orders

One Zombies supply drop each; three offers per week from seven options.

| Order | Objective |
|---|---|
| Precision Week | Kill 200 zombies with headshots |
| Well Equipped | Kill 150 zombies with equipment |
| Death Trap | Kill 150 zombies with traps |
| High Voltage | Kill 200 zombies with electrical damage |
| Fire Brigade | Kill 10 Brenners |
| Saw Specialist | Kill 100 zombies with the Ripsaw without its heavy attack on The Darkest Shore |
| Shore Leave Denied | Kill 750 zombies on The Darkest Shore |

## Contracts

Eight options, three active slots. Every contract awards one Zombies supply drop. Timers consume only time spent in a Zombies match. Original three kill contracts retain their targets, prices and timers.

| Contract | Objective | Match time | AC cost |
|---|---|---:|---:|
| Zombie Hunter | Kill 250 zombies | 50 min | 100 |
| Zombie Slayer | Kill 400 zombies | 60 min | 250 |
| Zombie Exterminator | Kill 750 zombies | 120 min | 450 |
| Airborne Blades | Kill 15 zombies with throwing knives while airborne | 15 min | 150 |
| Bestiary Bounty | Kill five different zombie types in one match | 30 min | 200 |
| Ripsaw Reaper | Behead 50 zombies with the Ripsaw on The Darkest Shore | 60 min | 300 |
| Freefire Frenzy | Kill 175 zombies while Freefire is active | 30 min | 350 |
| Treasure Hunter | Kill a Treasure Zombie | 180 min | 250 |

Bestiary Bounty uses the native five-species milestone flag; the UI counts one completed milestone. Accept it before starting the attempt. Ripsaw and Sub Pen objectives require The Darkest Shore. Brenner kills follow the native community-credit rules.

## Verification and follow-up

The catalog retains the exact filters from `dwgamechallenges.csv`. Descriptions were checked against locally extracted `_events_z` and `_achievement_engine_z_utils` reporting functions: weapon/enemy classes, bit flags, map IDs and the five-species milestone. Extracted game assets are not distributed here. Synthetic C++ tests exercise every rule through the production achievement engine; the Lua suite checks menu data against extracted tables. Live specialist-event delivery and co-op gameplay still need confirmation.

Revive, purchase and wave objectives, plus inconsistent retail predicates, are tracked in [CR issue #8](https://github.com/ChronoRixun/S2x/issues/8). No unfiltered wave, spending or special event has been added as a generic increment.
