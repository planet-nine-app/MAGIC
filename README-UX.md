# MAGIC

*MAGIC* is a way of getting devices to talk to each other and agree that some series of events and connections has happened. 

## Overview

*A note on currency* - This README makes reference to small-scale transactions.
Of course, most of the world doesn't use the same currency, and the volatility, and relative obscurity of cryptocurrencies make them unsuitable to the task of conveying some global sense of value.
I thought about using the price of a gallon of milk as a measure, but that doesn't work as a shared experience either.
I think the closest thing to a universal commodity is probably gas/petrol, and since four liters is roughly a gallon we can use that as the unit of measure.
Of course the price of gas/petrol varies widely, but the order of magnitude is the same...I hope.
And since this explanation is too wordy already, the comparison of values is between the value of an ad-supported page view, which I'll abbreviate as APV, and four liters of gas/petrol, which I'll abbreviate as 4LP. 

## Now the actual overview

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
If you consider some ad-laden site a transaction of an APV, because that's what ends up in the creator's pocket from a handful of visitors, then we really have no way of doing transactions between an APV and a 4LP.
Collectively these are called micro-transactions, and because of interchange fees, its rare for anyone other than governments to allow them (they're why shops have minimum transaction amounts).

My first professional developer job was working on transit systems where transactions often fell in this gap. 
Due to how our monetization was set up, we would actually lose money on any transaction under 4LP so we made a 4LP minimum. 
Thing is, when you just need to get on the bus, and you're down to your last half of a 4LP, this doesn't do you much good.
I've been there. 
If you've been there too, you'll probably understand the motivation for wanting to fill this gap with _something_, _anything_ that can help people out with those transactions.

I don't know how relevant this is worldwide, but in the US apartment buildings will often have coin operated washers and dryers.
In a world where no one carries cash this can be a whole day process: take the bus (gotta buy 4LP worth of tickets becase of our app lol) to the bank, get a roll of quarters, bus back, machine's broken so it eats your quarters, call the landlord, but he won't get back to you because it's the weekend, now all your clothes are wet, and you have a job interview tomorrow...
If this works I'll replace all those stupid coin operated washers and dryers with MAGICal gateways so people don't have to deal with that anymore.

## What does success look like?

I've thought about this for a long time.
Is this some path to world domination, or just a way for indie hackers to have something fun to play with?
I think I've arrived at the notion that just giving people the opportunity to use MAGIC, and see what, if anything, comes of it is good for now. 
We can iterate, and change the protocol as needed to make it more interesting.

So I think what would be great is for that "branded" UX to be recognized by some people. 
Seeing tiktoks of people waving wands, or some ad-free blog post about blog posts that use MAGIC, that would be success.
If it grows from there, I'll consider that a bonus.

## How do we get there?

So the purpose of this repo is to define the protocol, implement it in as many languages as possible, and provide examples in as many frameworks as possible. 
We want to create [this oauth page][oauth-page], but make it look like it wasn't made by a bunch of devs in 2004 
Based on what MAGIC can do, and what we're trying to do here, my thoughts on where we could use UX are:

* Seriously, what's the UX of MAGIC?

* What kind of UI kit can we provide for MAGICal interactions?

* How does a user know something is MAGICal?

## How to contribute

You are of course welcome to submit PRs, and issues, but you can also come chat with us over at
<a href="https://discord.gg/W4mQqNnfSq">
<img src="https://discordapp.com/api/guilds/913584348937207839/widget.png?style=shield"/></a>

I'm planetnineisaspaceship there.


[blog]: https://www.planetnineapp.com/blog
[oauth-page]: https://oauth.net/code/

