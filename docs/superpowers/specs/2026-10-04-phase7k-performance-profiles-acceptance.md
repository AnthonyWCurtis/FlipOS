# FlipOS Phase 7K Performance Profiles Hardware Acceptance

Date: 2026-10-04
Device: Miyoo Flip, RK3566

## Candidate

Hardware-tested code commit: `f50d3f1`

Deployed SHA-256:

```text
flip-frontend 1a918c5690a97f8dab08bae328a6d81b40bfafeba210eacda983f398a1638e86
flip-gb       f1b4430758028be0a0dc3cc20919c1b9501ea8f331d8bd84980b4997317a1c12
```

Phase 7J binaries were preserved on-device as rollback copies before replacement.

## Host gate

The exact deployed tree passed `make verify`.

Focused tests proved:

- profile naming and governor query;
- supported-governor detection;
- BALANCED / PERFORMANCE / POWERSAVE application;
- exact explicit restore;
- unsupported-profile non-destructive behavior;
- CPU rollback when a later GPU write fails.

## Verified hardware controls

The real Flip reported:

```text
CPU governors:
interactive conservative ondemand userspace powersave performance schedutil

CPU frequencies:
408000 600000 816000 1104000 1416000 1608000 1800000 1992000

GPU governors:
rknpu_ondemand dmc_ondemand vdec2_ondemand venc_ondemand userspace powersave performance simple_ondemand

GPU frequencies:
900000000 800000000 700000000 600000000 400000000 300000000 200000000
```

Starting governor pair:

```text
cpu=ondemand
gpu=simple_ondemand
```

## Hardware profile proof

The service probe produced:

```text
original cpu=ondemand gpu=simple_ondemand
supported balanced=1 performance=1 powersave=1
performance cpu=performance gpu=performance
powersave cpu=powersave gpu=powersave
restored cpu=ondemand gpu=simple_ondemand
```

A direct sysfs read after the probe confirmed the restore:

```text
cpu=ondemand
gpu=simple_ondemand
```

## Deployment regression

After deploying the verified binaries and rebooting:

- both deployed hashes matched;
- the frontend started normally;
- CPU remained `ondemand`;
- GPU remained `simple_ondemand`;
- no profile was applied implicitly.

The existing interrupted Pokemon Red session remained classified as interrupted; Phase 7K does not modify session policy.

## Scope result

Phase 7K is hardware accepted as a control service only.

No frequency limits, automatic per-system selection, thermal policy, battery-driven switching, PMIC changes or undervolting were introduced. Those remain Phase 10 Performance / Power work.
