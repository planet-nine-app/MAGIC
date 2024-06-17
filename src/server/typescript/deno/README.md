### Overview

This is a typescript implementation of Continue Bee written for deployment in the Deno ecosystem (it is dependent on Deno's key-value store, and uses Deno's included fetch server).
It is deployable as is to denoland via `deployctl deploy` [see deno deploy docs here].

There is a hosted version of it here: https://thirsty-gnu-80.deno.dev.

### WARNING

The hosted version of this Continue Bee server is for testing only, and may reset and/or disappear at any time. 
It should not be used for production.

### Testing

All of the tests in the test directory should be runnable against this server.
Simply update the url(s) in the tests to point to your server.

[see docs here]: https://docs.deno.com/deploy/manual/deployctl
