# tsf-flash

Flashing a device under test from a Test Agent — whichever way the
device takes firmware — packaged as an external Test Environment (TE)
repository and consumed with the `TE_EXT_REPO` builder directive.

Library:

- `tapi_flash` — engine-side TAPIs, built as a shared library:
  - `tapi_flash` — the image being written, the result of an operation,
    which flashing tools the agent has, reading a device back to verify
    a write, and waiting for a device across the reset into new firmware;
  - `tapi_flash_dfu` — USB DFU, with `dfu-util`;
  - `tapi_flash_fastboot` — the Android bootloader, with `fastboot`;
  - `tapi_flash_mtd` — a target's own raw block or MTD storage, from a
    shell on the target, with `dd`, `flashcp`, `nandwrite` and
    `flash_erase`;
  - `tapi_flash_rom` — SPI and BIOS flash chips, with `flashrom`;
  - `tapi_flash_esp` — Espressif chips, with `esptool`;
  - `tapi_flash_ocd` — anything on JTAG or SWD, with `openocd`.

It builds on
[tsf-devtool](https://github.com/interpretica-io/tsf-devtool), for
running a tool on an agent and capturing what it printed, and on
[tsf-kernel](https://github.com/interpretica-io/tsf-kernel), for the
issue list the verify and reappear checks report into.

## Usage

Declare the repositories in an external libraries catalog (e.g.
`conf/external.yml` in the test suite) and pass it to
`dispatcher.sh --external=external.yml`:

```yaml
repositories:
  - name: tsf_devtool
    url: https://github.com/interpretica-io/tsf-devtool.git
    ref: <tag>
    libs:
      - tapi_devtool
  - name: tsf_kernel
    url: https://github.com/interpretica-io/tsf-kernel.git
    ref: <tag>
    libs:
      - tapi_kernel
  - name: tsf_flash
    url: https://github.com/interpretica-io/tsf-flash.git
    ref: <tag>
    libs:
      - tapi_flash
```

Bind them to the engine platform in `builder.conf`:

```
TE_EXT_REPO_USE([tsf_devtool], [], [tapi_devtool])
TE_EXT_REPO_USE([tsf_kernel], [], [tapi_kernel])
TE_EXT_REPO_USE([tsf_flash], [], [tapi_flash])
```

and add `tapi_flash` to the `te_libs` list of the suite's `meson.build`.

Requires TE with `TE_EXT_REPO` support, and an **RPC** job factory
(`ta_rpcprovider` on the agent): everything here reads what a tool
printed, which needs output channels, and only that factory has them.

## Why

A suite flashes for two reasons: to put the build under test on the
device before the run, and to make flashing itself the thing under test —
that a bad image is refused, that an interrupted write recovers, that
the device comes back. Both want the same handful of operations — erase,
write, read back, verify, reset — over tools that share nothing else,
and that is what this gives them behind one shape.

## Where the tool runs

The tool runs on the agent behind the job factory. For an external
programmer — a DFU device on a USB port, a JTAG probe, an SPI clip —
that is the host the device is wired to, and the agent may be the engine
host itself. For `tapi_flash_mtd` the agent **is** the device under
test: it flashes its own partitions. Either way the image is a path on
that host; put it there first with TE's `tapi_file_copy_ta()`.

## Proving it landed

A tool that returns success has written what it was given; it has not
shown the flash holds it. Where the transport can read back — DFU
upload, `flashrom -r`, `dd` from the device, `esptool read_flash`,
`openocd dump_image` — `tapi_flash_verify()` reads the region and
compares its SHA-256 with the image's, hashing exactly as many bytes as
were written so a read back padded to an erase block still matches. Each
tool has a `*_write_verify()` that writes and then does this in one call:

```c
tapi_flash_dfu dfu = tapi_flash_dfu_default;
tapi_flash_image image;
bool matched;

dfu.vid = 0x0483; dfu.pid = 0xdf11; dfu.address = 0x08000000;
CHECK_RC(tapi_flash_image_init(factory, "/tmp/fw.bin", "bin", &image));
CHECK_RC(tapi_flash_dfu_write_verify(factory, &dfu, &image,
                                     "/tmp/readback.bin", &issues, &matched));
```

That verify is the check a flashing test should fail on, not the exit
status of the writer.

## Reporting

The operations return a `tapi_flash_result` and a status code, for a
test that flashes as a step and fails outright if it cannot. The checks
that belong to a flashing test — a verify that did not match
(`flash.verify`), a write that failed (`flash.write`), a device that did
not come back (`flash.no-return`) — append to the `tapi_kernel_issues`
list of tsf-kernel, so a flashing suite reports the same way as the rest
and its verdicts go into `conf/trc.xml`.

## Across a reset

After flashing, a device drops off its bus and comes back running the
new firmware. `tapi_flash_wait_reappear()` watches for that round trip
through a caller-supplied "is it visible now?" callback — a USB id
listed, a path present — and reports `flash.no-return` if it left and
did not return. A reset too quick to catch is not a failure.

## Scope

Flashing writes to hardware and can leave a device that will not boot.
Everything here points at the device the suite's own configuration
names; that is the engagement it belongs to.
