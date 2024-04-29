# MAGIC

*MAGIC* is a backronym for Multi-device Asynchronous Generic Input/output Consensus.

Yes, seriously. 

It is, in essence, a generalization of the process by which bank settlement happens with debit and credit card purchases. 
Though purchasing is one of its use cases, MAGIC can allow for a whole host of interesting interactions between that have never interacted before, and need not interact again.
It is always a little contrived to call technology magic, but as David Copperfield said at a show of his I went to as a kid, "we all know that magic isn't real, but if it was, here's what it would look like."

<MAGIC video>

## Overview

Most transactions in cyberspace happen between a client and a server.
Usually the client is either a computer or a phone, and the server is software running on some amorphous collection of hardware living in some data center somewhere.
It's not uncommon for the server to receive a request, and hand off some or all of it to a trusted third party through the use of an API token, or user-based authentication like OAuth2.0. 
MAGIC uses [Sessionless][sessionless] to enable trust in the third party from the _client_ instead of the server.

Wihtout needing the client to establish a connection with the server to use the third party, the server can actually skip authenticating the client, and simply passthrough requests to the third party.
As stated above, this is how payment processing at store with a POS works. 
The buyer presents a request via their credit card, the POS machine, which knows nothing of the buyer, forwards that request to a trusted third party, the credit card company, who in turn notifies the banks, and if everything is good, the transaction settles, the POS machine receives a successful response, and the shopowner completes the transaction.

Here's a picture of that process:

```mermaid
sequenceDiagram
    Humans->>+POS: Buyer presents credit card
    POS->>+AcquiringBank: POS sends cc details to bank to be paid
    AcquiringBank->>+IssuingBank: Bank to get paid sends details to bank that issued card
    IssuingBank->>+AcquiringBank: Issuing bank says funds are good/bad
    AcquiringBank->>+POS: POS receives success/failure message
```

So in this transaction the humans do something, the POS reads a card and gets a token, that token is sent to a bank, and decoded then sent to another bank, which approves the transaction. 
The approval then gets sent back the other way, and all the pieces agree on whether it succeeded or failed. 
The card networks (Visa, MasterCard, Discover, Amex) provide the routing between banks. 
Merchants get charged a flat percentage or fee on the transaction, something like 2.5-3.5%, and the card networks and banks all take a slice and everyone is happy.

This whole system was pretty advanced in the early 70s when it was first computerized. 
But it's fifty years later, and magnetic strips aren't pushing the envelope anymore.

## The internet

Twenty years after credit cards were computerized, a new problem faced the banks.
A fledgling network of computers was growing, and people were wanting to spend money on each other's goods.
A company called First Virtual Holdings, Inc. were the first to recognize that a solution for credit card payments was necessary for the web.
A few years later, Peter Thiel and Max Levchin started Confinity, which merged with Elon Musk's X.com, then renamed itself to PayPal, and was acquired then by eBay, a series of events which has pretty consistently made the argument that maybe people shouldn't be billionaires ever since.

Why do we need PayPal? 
Because our browsers, and thus the web is inherently insecure, and credit cards are pretty hilariously insecure.
So the original solution was some UX nightmare of email confirmations. 
Nowadays we have slick UIs that are dropped into websites and apps where people can enter credit card numbers with minimal threat of them getting stolen. 

When you do this, your card gets tokenized, and stored for future use if you want. 
And here's where MAGIC comes in.
Because there's no reason why that token needs to be trapped in a single company's database.
All you need is a way of associating your account in one app, with the account in the app where the token is stored.
Then you can use that token for purchases, and don't have to keep re-entering your card.

Let's take a look at that picture:

```mermaid
flowchart TD
    A[Join app 1] 
    B[Join app 2]
    A --> C{Associate apps}
    B --> C{Associate apps}
    C --> D[Make Purchase]
    D --> |send card token| E[payment processing]
    E --> F[Payout to merchant]
```

So long as the merchant can be linked via the processor, everyone gets their cut, and everyone's happy.

So great, we can make purchases from one app in another app. 
That's kind of interesting, but nothing earth shattering. 
But what about this picture:

```mermaid
flowchart TD
    A[Join app 1] 
    B[Join app 2]
    A --> C{Associate apps}
    B --> C{Associate apps}
    C --> D[Make Purchase]
    D --> |send card token| E[payment processing]
    E --> F[Payout to merchant]
    E --> G[Payout to deliverer]
```

This picture's a little more interesting. 
Now you have the user's payment split some way between the merchant and a delivery service (say something needs to be shipped, or food is getting delivered).
So long as the user, merchant, and delivery service are all tokenized by the same processor, this works.

Of course tokenizing with the same processor has historically meant they're all part of the same company.
But remember with Sessionless, we can associate accounts. 
So so long as implementers can resolve the keys involved in the transaction back to accounts tokenized with the same processor, everyone gets paid.

And that's what MAGIC does.
It allows implementers to construct payment pipelines that initiate from a user action, which can be as simple as tapping a button, to as interesting as waving a wand and saying a magic word, and process an arbitrary set of inputs to be resolved in such a way that all participants agree. 
Just like payments have always done, just with the ability to add actors without a bunch of account management.

## What about non-payments?

When I first thought of MAGIC (you can read the [original patent here][magic-patent]). 
I was actually more excited about its potential as a solution to a problem that had plagued me for years.
Namely as a way to facilitate micro-transactions.

I wrote a whole blog post about it that no one read, and a few people on Reddit made fun of me for before my post got deleted.
If you want you can check it out here: [micro-transaction blog post][blog]. 
What I didn't realize at the time was that I was too high up in the stack, and hadn't explained the three layers underneath.
That sent me back to the drawing board, and onto this current effort to get this stack out into the world. 

So forget about the micro-transaction part right now, let's just talk about what a non-payment MAGIC pipeline means, and what it can do.

I wanted it to feel magical, and at the same time needed a way to prevent spamming requests since any kind of "free" reward on the internet is just begging to be hit by bots.
This was easy enough, let's introduce a globally available resource called Power (now I think we should just drop the pretense and call it MP), that recharges over time, and caps just like MP does in a game like Skyrim.
This provides a cost for magical transactions, and fits the lore quite nicely. 

It also has some interesting properties.
MP is non-transferrable, so when you spend it it just goes away only to recharge over time.
That means people can't buy or trade MP.
It also means, unlike money, there's an opportunity cost to not spending MP since if it maxes out, you stop accruing it.

This opportunity cost is really interesting from an economic standpoint since it's the _opposite_ of what is happening in the world of crypto.
Since Bitcoin et al's value tends to go up faster than inflation, the lowest value it has is when you acquire it, and so there's no incentive to spend it.
This makes it great as a store of value, but lousy as a medium of exchange. 
Could MP provide the counterpoint to crypto, and be that medium of exchange?

[I said I wasn't going to go there, but here's the link to that blog post again.][blog].

Put the money aspect aside again for a moment.
Since MP doesn't transfer, something of some sort of value needs to be transacted. 
So I made up a token I call Nineum, and made up some rules for its distribution. 
[Nineum is non-fungible tokens done right, and in such a way to (eventually) enrich their owners.][nineum]

## So what is MAGIC again?

So MAGIC (Multi-device Asynchronous Generic Input/output Consensus) is a protocol for getting multiple computing devices to agree that a transaction has taken place.
That transaction can involve money, or involve MP.
The protocol is open sourced, and available under a broad license so that anyone can use it so long as they agree that their MAGIC transactions be interoperable with other systems' MAGIC transactions.
That way additions to the protocol benefit all the users of the protocol. 

Below are links to the developer and UX READMEs for MAGIC:

| Dev          | UX          | Product     |
|--------------|-------------|-------------|
| [README-DEV] | [README-UX] | coming soon |

[sessionless]: https://www.github.com/planet-nine-app/sessionless
[magic-patent]: https://www.planetnineapp.com/magic
[blog]: https://www.planetnineapp.com/blog
[README-DEV]: ./README-DEV.md
[README-UX]: ./README-UX.md
