# MAGIC

*MAGIC* is a protocol that genericizes the multi-platform payment settlement flow to allow for both money and moneyless transactions between multiple untrusted clients, servers, and peers.

## Overview

This repository contains implementations of the MAGIC protocol in various languages, on various platforms, and, since multiple devices are involved in MAGICal transactions, via various transports.
This is a protocol enabled by the asymmetric cryptography of the [Sessionless protocol][sessionless].
If you're unfamiliar with Sessionless, I encourage you to check out its repo, or one of its implementations before diving into MAGIC.

In a standard client/server relationship, clients make requests to a server, which authenticates the client to make the request, and then serves a response.
This relationship is 1:1 because the traditional method of authorization is via a shared secret between client and server.
Sometimes communication to a third party is facilitated via server to server communication authorized by API key (another type of shared secret), or via OAuth2.0, which is a flow that generates, you guessed it, a shared secret. 

Let's imagine a system with multiple such integrations:

```mermaid
flowchart LR
    A[Client]-->|Makes Request| B{Server}
    B -->|Authenticates| B
    B <-->|Api Key| C[3rd Party Provider]
    B <-->|OAuth2.0| D[Another 3rd Party]
    B <-->|Cryptography| E[A 3rd 3rd Party]
    B -->|Responds| A
```

As you can see the server acts as the hub for all of these integrations, and for many of us who develop for a living, adding the line from third party provider to client is what we spend a lot of our time doing.

With Sessionless, however, no shared secret is necessary anywhere along the pipeline. 
This makes a couple of interesting interactions possible. 
First, Sessionless-enabled clients don't need to rely on a single server for authenticated requests. 
Let me show you that picture:

```mermaid
flowchart LR
    A[Client]<-->|Makes Request| B{Server}
    A<-->|Makes Request| C{Server}
    A<-->|Makes Request| D{Server}
    A<-->|Makes Request| E{Server}
```

If you think about a federated network like the Fediverse, this type of picture would let you build a client that can display Mastodon, Pixelfed, Lemmy, etc, all in one *without* having to set up your own server that combines all of them. 
Just find Sessionless enabled servers for them (or add Sessionless to existing ones), and that client is possible.

I think this is a great use case for Sessionless, but it's not what we're going to call MAGIC.
Instead, I'd like you to consider this picture:

```mermaid
flowchart LR
    A[Client]-->|Makes Request| B{Server}
    B-->|Passes Request| C{Server}
    C -->|Authenticates| C
    C -->|Response| B
    B -->|Adds To Response| A
```

Here the client passes an authenticated request to a server, which in turn passes that request to a second server for authentication.
If the signed message sent from the client is what is signed, then the middle server can't modify the request, and have it be authenticated by the server on the right.
At the same time, if the middle server is known to the server on the right it can add its own signed message to the request for authentication. 
The server on the right, authenticates one or both messages, and sends back a response, which allows the middle server and the client to respond to the success.

Since the server on the right is known to both the client and middle server, the client and middle server need not establish a trust relationship before this interaction.
And *that* is what we call MAGIC.

Some terminology.
In a moneyed interaction, this flow could be called a transactional pipeline, but since MAGIC enables this flow when nothing is actually exchanged, a broader term is needed.
And since I want to stay on brand, let's call it a spell.


* The initiator of a spell is the caster.
* The first receiver of a spell is the spell's target.
* The server that authenticates all the messages is the spell's resolver.
* The intermidaries (of which there can be an arbitrary amount) of a spell are called gateways.
* Targets are gateways.
* Resolvers are gateways. 
* The response to a spell is its effect.
* Effects may affect any number of the gateways involved.
* Effects always modify something in either the caster, the target, or both.

So the MAGICal interaction above gets renamed as such:

```mermaid
flowchart LR
    A[Caster]-->|Casts Spell| B{Target}
    B-->|Passes Spell| C{Resolver}
    C -->|Resolves| C
    C -->|Effect| B
    B -->|Adds To Effect| A
    A -->|Modifies State| A
```

## Use Cases

Consider a simplified food delivery app like DoorDash. 
We want users (casters), to be able to order (cast) from restaurants (targets), get their food delivered from some unaffiliated group of drivers (gateways), and have money disbursed from the user (caster) to the restaurant and driver (gateways) from some server that knows about all of them (resolver).

Let's look at that picture:

```mermaid
flowchart LR
    A ~~~B[Restaurant A]
    A[User <Caster>]-->|Orders| C[Restaurant B <Target>]
    A ~~~D[Restaurant C] 
    C -->|Books| E[Driver A <Gateway>]
    C ~~~ F[Driver B]
    C ~~~ G[Driver C]
    E --> |Passes Spell| H[Resolver]
    H --> |Authenticates Spell| H
    H --> |Driver Is Booked| E
    E --> |Order Is Placed| C
    C --> |User Starts Tracking Order| A
```

Now in current food delivery apps, the user, restaurant, driver, and resolver are all part of the same platform.
That's great for the platform, and not so great for everyone else involved.

Now I'm not going to say anything hyperbolic like we can replace every platform with decentralized pieces that can be Voltroned (It's an unfortunate reality of being alive that not everyone will get your cultural references.
Voltron was an animated American show in the eighties that featured a group of teenagers fighting aliens in animal-shaped robot spaceships.
[They could combine these spaceships into one giant spaceship robot called Voltron][voltron].
To five year old me it was about as awesome a thing as could occur) together to replace every centralized platform out there.

But with MAGIC we could do that.

Here are some other use cases:

* Ridesharing
* Order placement (why on Earth do we have to enter our address _every_ single time we order from a new site?)
* Multi-client pipelines like phone -> TV app, or phone -> gaming console
* POS systems
* Novel monetization, and signup systems for saas platforms

But moneyed pipelines are only one side of the use case story.
What I think is arguably even more interesting is what MAGIC can do for systems that want interaction, but don't need money to change hands. 
First, what kind of interaction is that?

Let's get a little pie in the sky. 
Imagine you're watching a streamer playing a game on some unnamed streaming platform on your TV, and they get to a tough boss fight, and you want to send them an extra health potion to use during the fight. 
Let's take a look at what would need to happen for that to happen:

```mermaid
flowchart LR
    A[User's Client <Caster>] -->|Requests <Casts>| B[TV <Target>]
    B -->|Passes Request| C{Streaming Server <Gateway>}
    C -->|Passes Request| D[Streamer's Client <Gateway>]
    D -->|Passes Request| E[Game Client <Gateway>]
    E -->|Passes Request| F{Server <Resolver>}
    F -->|Effect| E 
    E -->|Effect| D
    D -->|Effect| C
    C -->|Effect| B
    B -->|Effect| A
```

Now throwing your favorite streamer a health potion in a game is a pretty cool interaction, and it's one that to the best of my knowledge doesn't exist right now.
Why?
Well fundamentally letting user A (the user watching the stream) do something in user B (the streamer)'s client is just not done.
For one it requires several heavy handed integrations to do with traditional auth systems, which might be ok for moneyed transactions, but just doesn't make sense for unmoneyed transactions.
For two, traditional auth can't pass authenticated requests through multiple clients and servers, but asymmetric cryptography like that used in Sessionless can.

Notice that the streaming clients and server don't need to *do* anything other than pass the request and the effect along between the caster and resolver. 
This means to implement this, the game doesn't need to worry about what streaming platforms it needs to know about, and the casting client doesn't need to know about what games it works with.
Figuring out how to communicate that to the user is a [UX question][ux], but the fact that you can create experiences like this without integrations all over the place is pretty magical to me.

Here are some other unmoneyed use cases:

* Opt-in interactions (sharing email, address, getting promos/deals)
* Interactions proxied through extensible clients (chat clients with bots like Slack and Discord, content aggregators with APIs like Reddit and Twitter)
* Accountless interactions on content sites like blogs
* Interactions with media through clients not owned by that media

## Getting started

> **Note**: This repo is a work in progress.

This repo is organized by language.
Each language is divided into transport implementations like https, websocket, and ble. 
For languages with clients, there are both client and server implementations.
If you want to implement this system in your app, you'll likely want to start at your language's package manager ([npm], [CocoaPods], [Maven], etc). Links for those can be found at the end of this doc.

It wouldn't be MAGIC without spells.
While MAGIC defines the overarching protocol for constructing multi-device authenticated requests, spells define the specific properties required from each participant in a pipeline to resolve that spell.
Let's take a look at our food delivery clone again.

The MAGIC protocol requires from the caster:

```json
{
  timestamp: string,
  casterUUID: string,
  totalCost: non-zero int, 
  mp: bool,
  ordinal: int,
  casterSignature: signature,
  gateways: []
}
```

Where `timestamp` is the time of the transaction as a string in milliseconds from epoch.
`casterUUID` is the uuid of the caster.
`totalCost` is the non-zero cost of the transaction (zero cost transactions allow for MAGICal abuse).
`mp` is truthy for non-moneyed transactions, and false for moneyed transactions (currencies for things are resolved by resolvers).
`ordinal` is a number that increments with every spell that resolves.
This makes spells idempotent, and diminishes vulnerability to replay attacks.
`casterSignature` is the signature of this original message from the caster.
`gateways` starts as an empty array, and is where the target, and various gateways will add themselves to the pipeline.

Next comes the restaurant who adds their details to the payload:

```json
{ 
  timestamp: string,
  casterUUID: string, 
  totalCost: non-zero int,
  mp: bool,
  ordinal: int,
  casterSignature: signature,
  gateways: [{
    timestamp: string,
    uuid: string,
    minimumCost: non-zero int,
    ordinal: int,
    signature: signature
  }]
}
```

`minimumCost` is the minimum amount that needs to resolve to the restaurant to complete the delivery.
Because gateways may not have pre-negotiated fees, the resolver needs to know their requirements for fulfilling a spell.
In this instance, if the amount a delivery driver is looking for exceeds the amount the restaurant is willing to pay, then the spell doesn't resolve and the order isn't fulfulled.

Next we'll add the driver:

```json
{
  timestamp: string,
  casterUUID: string,
  totalCost: non-zero int,
  mp: bool,
  ordinal: int,
  casterSignature: signature,
  gateways: [{
    timestamp: string,
    uuid: string,
    minimumCost: non-zero int,
    ordinal: int,
    signature: signature
  },{
    timestamp: string,
    uuid: string,
    minimumCost: non-zero int,
    ordinal: int,
    signature: signature
  }]
}
```

And that gets passed to the resolver who, in this case, makes sure the minimumCosts are less than totalCost (further price negotiation is left to implementers), makes sure that the uuids of the gateways fulfill the roles of the spell (that one's a restaurant, and one's a driver), checks the signatures against their respective messages, and then sends back yay or nay based on all of that.
When the driver receives yay they start the delivery process.
When the restaurant receives yay, they start preparing the food.
When the caster receives yay, their client can start the tracking UX.

The above is the minimum for a MAGICal spell, but spells can add whatever optional fields they want for their resolution.
Let's say for example you wanted to make sure the driver was within five miles of the restaurant when the spell resolves (you'd want to take care of this before it got to the resolver in a real system, but using this for an example because it's easy to imagine sending geo data). 
Your reolvable payload could then become:

```json
{
  timestamp: string,
  casterUUID: string,
  totalCost: non-zero int,
  mp: bool,
  ordinal: int,
  casterSignature: signature,
  gateways: [{
    timestamp: string,
    uuid: string, 
    minimumCost: non-zero int,
    ordinal: int, 
    signature: signature
  },{ 
    timestamp: string,
    uuid: string,
    minimumCost: non-zero int,
    ordinal: int,
    **currentLocation**: geolocation,
    signature: signature
  }]
}
```

`currentLocation` could then be checked against the restaurant's location if it's stored. 
If it's not stored, the spell can just require the restaurant to send its location as well.

My hope is that, like spells in fantasy works, these spells become something written down and shared for practitioners of MAGIC to learn and use.

## mp?

The astute observer of the MAGICal JSON above will have noticed this abbreviation, which has very intentionally been used to invoke the sense of magic power as used in games.

With spells that don't cost money, you have to worry about computers spawning millions of requests.
*Especially* if the interaction gives anything of value.
This isn't much of a problem with standard systems, because requests are gated by the system's authentication, but in a MAGIC spell, those requests might pass through any number of gateways.
Since the effect on the gateway of that is unknown, responsible casters shouldn't spam the gateways, but since it's the internet and you can't just boot irresponsible casters, you've got to do something else instead.

So we use Magic Power (MP).

[sessionless]: https://www.github.com/planet-nine-app/sessionless
[voltron]: https://www.youtube.com/watch?v=2M3cyCFWChg
[ux]: https://www.github.com/planet-nine-app/MAGIC/README-UX.md
[cocoapods]: https://cocoapods.org
[maven]: https://maven.apache.org
[npm]: https://npmjs.org
