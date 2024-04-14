# MAGIC

*MAGIC* is a way of getting devices to talk to each other and agree that some series of events and connections has happened. 

## Overview

The main README talks a lot about how MAGIC can be used for traditional moneyed transactions, and we'll talk about that some here, but what I really want to talk about is non-moneyed transactions. 
Transactions that are truly MAGICal, and how we can make them _feel_ magical.

There are two types of MAGICal transactions, ones that happen within a single device such as with a website or app, and ones that happen between devices. 
For the former, like any other transaction, we want some kind of confirmation that the transaction completed, but I think we can do better than green check marks.
For the latter we likewise want some confirmation, but that confirmation may have to be on another device. 
And eventually the idea is that the secondary devices involved might be able to do things more interesting than just displaying that confirmation. 

MAGIC opens up a world of new interactions between devices. 
It enables new types of hardware, which can participate in transactions without relying on cards and taps.
And while MAGIC itself isn't a brand, the hope is that as it spreads we can bring some semblance of branding to it so people know when they are doing something MAGICal.

## The problem

In the top README I focused on the problems with money transactions, so here I'll focus on the problem for non-moneyed transactions. 
The problem MAGIC is looking to solve with non-moneyed transactions is that currently there is too large a gap between "free" (ad supported) interactions online, and paid interactions.
To solve this, non-moneyed transactions need to provide value to spender and spendee.
What that value is is covered up higher in the stack. 
For now let's just assume there's some answer to that, and ideate what it looks like (or if you want to check out my attempt at an answer you can [read about it here][blog]).

So how can we get across that there's a new way to interact with content online, and what does that whole experience look like for creators?
What can creators _do_ on their sites, or in their media with MAGICal engagement?

The biggest problem you have with any thing that gives value for "free" on a computer, is that if the thing is valuable enough, people will create bots to get that thing.
The easiest way to combat this is to gate it by something that does cost money, which is why there aren't a million coupon bots running around.

But we want non-moneyed transactions to still have some value for everyone so we need to combat the bots.

The first mitigation is the introduction of MP (MAGIC Power). 
MP works like Magicka in the game Skyrim where you're allocated some amount, that amount is spent when you cast a spell, and it recharges back up to the allocated cap.
This makes sure that a single bot can't spam transactions by introducing a cost to them.

The second mitigation happens on the other side of transactions, namely that whatever is produced is limited in its ability to be aggregated and transferred to a single owner. 
This doesn't mitigate collusion, but it can limit the amount of fake users that are valuable in the system.

The third mitigation though is the one for UX to think about, which is how can we introduce a CAPTCHA (forget boring picture CAPTCHAs, we'll come up with something cooler) mid transaction to flag users as bots?

## The cooler problem

I mean it's called MAGIC. 
I've tried to make it seem like what fans of fantasy books and games would recognize as MAGIC mechanically.
So how can we make it _feel_ like magic.

## Origin and motivation

Here's where you might actually want to read that [blog post][blog].
The tl;dr is what I was talking about with the gap between advertising and paid transactions.
If you consider some ad-laden site a transaction of a few cents, because that's what ends up in the creator's pocket from a handful of visitors, then we really have no way of doing transactions between 

[blog]: https://www.planetnineapp.com/blog

