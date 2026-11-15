# Character generation: Rodin prompts and handoff

Written 2026-11-14. The tool is **Hyper3D Rodin only** (hyper3d.ai), including its own rigging/skeleton. The user gets lost in web UIs, so a Claude session with **computer use** (Claude Desktop → Code tab) drives Rodin while the user watches.

## Handoff: what the computer-use session should do

1. Open **hyper3d.ai** in the user's Chrome. The user is already signed in, with a Rodin Business subscription.
2. Generate **Enemy 2 (Suit goon)** first, as the pipeline test. Use the prompt below plus the shared block.
3. Settings: **T/A pose ON**, material **PBR**, mesh **Quad**, about **30k faces**, texture **2K**.
4. Generate, and pick the preview with the cleanest hands and face. Then use **Rodin's rig/skeleton option** (humanoid).
5. Export **FBX with the skeleton**, no animation. If the FBX has no textures, also export a **GLB**.
6. Save it as `C:\Users\ng440\Desktop\Game\ArtDrop\Characters\Enemy_SuitGoon.fbx`, and the GLB next to it if made.
7. Stop. The Unreal side imports it, checks scale and skeleton, retargets it to the UE5 mannequin, and puts it on the enemies (`AEnemyBean` skin). Only after that check do the other 9 characters get generated.

Never put real names of films, actors or games in a prompt. These characters are original.

## Shared block: paste after every character prompt

```
Single character, full body, front view, A-pose: arms straight and angled 45 degrees down away from the body, palms facing down, fingers slightly spread, thumbs apart, legs straight, feet shoulder-width apart flat on the ground, symmetrical, neutral face, mouth closed. Empty hands, no weapon, no props, no base, no background. Stylized realistic game art, painterly hand-painted textures, clean simplified forms, matte materials, even neutral lighting, no baked shadows or highlights in the texture.
```

Negative prompt, if there is a field for it (for the dancers, delete `hat, cap, hood, long coat`):

```
hat, cap, hood, long coat, cape, backpack, weapon, gun, extra fingers, fused fingers, text, logos, base, pedestal, multiple characters, cropped body, blurry face
```

## Contract rules for enemies (FPS-Agent contract §6.1)

- **Size:** about 176 cm tall, with a normal head (about 24 cm across, between 146 and 170 cm high). The skin must match the bean's hitboxes.
- **Silhouette:** nothing large sticking out of the body: no hats, no flared coats.
- **Accent:** every enemy wears the same **acid-green** accent, so players can spot enemies in a strobe-lit crowd. Dancers never wear acid green.

## Player: "The Cleaner" (working name)

First person, so the gloves, cuffs and sleeves matter most.

```
Full-body 3D game character, adult man in his early 40s, 176 cm tall, lean athletic build, realistic adult proportions. Tailored double-breasted charcoal-black wool suit jacket, buttoned, sharp shoulders, slim black trousers, crisp white shirt, narrow black tie slightly loosened. Black leather gloves with a visible stitched seam across the back of the hand. Heavy brushed-steel wristwatch on the left wrist. Shirt cuffs show a thin deep-crimson inner lining as his only color accent. Polished black oxford shoes. Close-cropped dark hair with one grey streak at the left temple, clean-shaven, thin pale scar through the left eyebrow, calm stern expression.
```

## Enemies (3 mob variants)

**Enemy 1: Bouncer** → `Enemy_Bouncer.fbx`

```
Full-body 3D game character, heavy-set adult man, 176 cm tall, thick neck but shoulders no wider than an average man, realistic adult proportions, normal head size. Shaved head, short dark beard, flat broken nose, bored hostile expression. Tight black crew-neck t-shirt, black tactical cargo trousers, black combat boots laced high. Bright acid-green coiled earpiece wire behind the right ear and a bright acid-green fabric armband on both upper arms. Silver ring on the right hand.
```

**Enemy 2: Suit goon (FIRST)** → `Enemy_SuitGoon.fbx`

```
Full-body 3D game character, adult man in his 30s, 176 cm tall, average build, realistic adult proportions, normal head size. Slicked-back black hair, thin moustache, sneering expression. Cheap shiny dark-purple two-piece suit, jacket open, black shirt unbuttoned at the collar, thick gold chain, black pointed loafers. Bright acid-green pocket square and a bright acid-green fabric armband on both upper arms. Gold watch on the right wrist.
```

**Enemy 3: Enforcer** → `Enemy_Enforcer.fbx`

```
Full-body 3D game character, wiry adult man in his 20s, 176 cm tall, realistic adult proportions, normal head size. Buzz-cut bleached hair, neck tattoos, gaunt cheeks, aggressive expression. Dark charcoal zip-up track jacket with bright acid-green stripes down both sleeves, zipped to the chest, white undershirt, dark grey joggers, chunky white sneakers. Bright acid-green fabric armband on both upper arms.
```

## Dancers (6 "scummy" club-goers): muted dirty colors, never acid green

These files are `Dancer_01.fbx` … `Dancer_06.fbx`, in this order.

```
Full-body 3D game character, sweaty skinny young man, 172 cm tall, realistic adult proportions. Messy bleached hair, faded cyan bucket hat, glazed half-closed eyes. Oversized stained grey tank top, baggy faded cargo shorts, dirty white sneakers, glow-stick bracelets in pink and purple. Shiny sweaty skin. Muted dirty colors, no green.
```

```
Full-body 3D game character, young woman, 166 cm tall, realistic adult proportions. Smeared black eyeliner, messy dark-red bob haircut. Torn silver sequined mini dress, ripped black fishnet tights, scuffed black platform boots, many cheap bracelets. Tired wasted expression. Muted dirty colors, no green.
```

```
Full-body 3D game character, overweight middle-aged man, 175 cm tall, realistic proportions. Balding with greasy comb-over, sunburned face, red nose, sloppy grin. Stained unbuttoned Hawaiian shirt in faded orange and brown, white undershirt with a sweat stain, wrinkled khaki trousers, worn brown loafers. Muted dirty colors, no green.
```

```
Full-body 3D game character, pale androgynous young adult, 170 cm tall, realistic adult proportions. Dyed purple undercut hair, black lipstick, multiple facial piercings. Black mesh long-sleeve top, black vinyl trousers, silver chains on the belt, heavy black platform boots. Muted dark colors, no green.
```

```
Full-body 3D game character, stocky man in his 40s, 174 cm tall, realistic proportions. Greying mullet, stubble, tinted aviator sunglasses pushed up on the head. Faded blue denim jacket covered in patches over a stained band-style t-shirt with no readable text, ripped jeans, worn cowboy boots. Muted dirty colors, no green.
```

```
Full-body 3D game character, tall thin woman, 172 cm tall, realistic adult proportions. Wild teased platinum hair, heavy glitter makeup running down her cheeks. Matted pink faux-fur coat worn open over a black slip dress, laddered tights, one broken high heel. Muted dirty pastel colors, no green.
```

## Later: animations

Dances and panic animations (running, terrified, cowering) come after the first character is in the game. Prefer Rodin's own animation options if it has them; if not, Mixamo is the free fallback.
