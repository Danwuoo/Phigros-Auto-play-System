# Explicit production cancellation Up audit

Immutable source-3f23faf Hold diagnosis `prelive-production-journal-normal03`
contains 63 successful endpoint-free receipts. 59 have an exact accepted plan
step. The remaining four are Up receipts for intents 6, 14, 15 and 21. Each has
an explicit `game_contact_cancelled` row: own active Hold, current object or
region lost, no retry, old body evidence retained and cursor 1. Source and
positions match that contact's accepted body plan and verified Down. These are
safe cancellation releases, not normal tail success or verified game effects.

The immutable v3 audit and its STOP remain. A separate v4 audit retains its
full raw/schema/SHA/producer/packet/quantile checks and adds a strict cancellation
path only for an Up of an already verified own contact. It requires the latest
accepted body source, the exact last executed position, still valid body
evidence, current packet QPC containment and an unused unique explicit active
Hold cancellation. Down and Move cannot use that path. Every active cancellation
must have one release receipt. All receipts additionally prove unique Down,
no stolen contact, ordered QPC and own-contact Move/Up; the final verified
release must cover any remaining own contact.

The independent controls reject forged identity, contact, position, source,
evidence, prefix, clock, retry, cancellation reuse and game-effect claims.
The initial scope is the observed current-object/region-loss cancellation;
other unplanned receipts remain rejected until their contract is independently
covered. No numeric threshold changes. Any new v4 PASS is a new receipt and
never overwrites the v3 failure. A single synthetic production run is no AA,
cost comparison, real capture/RPC or physical qualification. Live stays closed.
