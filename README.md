# linux-live

This repo is used to build the Triton Datacenter Linux platform image. This is
not a standalone product and is intended to be used with Triton Datacenter.

**THIS IS A TECHNOLOGY PREVIEW.** Not everything works as expected yet. But you
(yes, you!) can help shape the direction of the product by getting involved.

Detailed documentation can be found in the [docs](docs) directory. Some of this
is likely to become offical documentation and some of it is capturing current
areas of exploration.

* See the [platform image][doc-pi] documentation for build
  instructions.
* See the [quick start][doc-qs] to get Linux CNs up and running quickly on
  Triton.
* See [Triton documentation][triton] for setting up a Triton Datacenter.

[doc-pi]: docs/2-platform-image.md
[doc-qs]: docs/6-quick-start.md
[triton]: https://github.com/TritonDataCenter/triton

## Bugs

If you find bugs with the platform image, please report them in this repo. If
you find bugs in the Triton API stack, please file them in their respective
repo (you can find a list in the [triton][triton] repository).

You can also find bugs filed in our internal Jira here:

* [Triton APIs][bugview-linuxcn-tag]
* [Platform Image][linux-pi-google]

[bugview-linuxcn-tag]: https://smartos.org/bugview/label/linuxcn
[linux-pi-google]: https://www.google.com/search?q=LINUXCN+inurl%3Asmartos.org%2Fbugview

## Getting Help

You can reach out to us in the following ways:

* [SDC Discuss][ml] mailing list
* `#triton` on [Libera Chat IRC network][libera]

[ml]: https://smartdatacenter.topicbox.com/groups/sdc-discuss
[libera]: https://libera.chat/
