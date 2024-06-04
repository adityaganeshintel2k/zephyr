Intel DSP Panther Lake Simulator
*******************************

Environment Setup
#################

Start with a Zephyr-capable build host on the Intel network (VPN is
fine as long as it's up).

The Zephyr tree is best cloned via west.

.. code-block:: console

   mkdir ~/zephyrproject
   cd ~/zephyrproject
   west init -m https://github.com/intel-innersource/os.rtos.zephyr.zephyr
   west update


Toolchain Setup on Host
=======================

As the Zephyr SDK doesn't support PTL, you will need to install the XCC
toolchain for this core. At least 2 files are needed:

- ace30_LX7HiFi4_linux.tgz
- XtensaTools_RG_2021_8_linux.tgz

You can get these, as well as others, by downloading the FMOS team's
all-in-one package for CAVS, MTL, and PTL boards:

- xtensa-dist.tar.gz

You can download it from http://gale.hf.intel.com/~nashif/audio/, along with
the simulator; see the next section.

With the above package, set the toolchain up:

.. code-block:: console

   # Uncompress the Xtensa toolchain
   tar zxvf xtensa-dist.tar.gz

   # Enter the directory
   cd xtensa-dist

   # Run the install script
   ./install.sh

The script will install the toolchain for you and set up the core registry
required by the toolchain. The toolchain will be installed in ~/xtensa.

You might need to install some 32-bit libraries to run this command and some of
the binaries included in the toolchain. Install all needed 32 bit packages:

.. code-block:: console

   sudo dpkg --add-architecture i386
   sudo apt-get install libncurses5:i386 zlib1g:i386 libcrypt1:i386

Make sure also that you have these additional libraries installed:

.. code-block:: console

   sudo apt-get install libncurses5 libncurses6 libncursesw5 libncursesw6 libtinfo6 libcrypt1 libssl-dev

Once installed, you are ready to build and run a Zephyr application for your board
using the Cadence XCC compiler and the software simulator.

A few environment variables are needed to tell Zephyr where the toolchain is:

.. code-block:: console

   export ZEPHYR_TOOLCHAIN_VARIANT=xt-clang

   export XTENSA_TOOLCHAIN_PATH=$HOME/xtensa/XtDevTools/install/tools
   export TOOLCHAIN_VER=RI-2021.8-linux
   export XTENSA_CORE=ace30_LX7HiFi4

You may also need to define the license server for XCC. Set the environment
variable `XTENSAD_LICENSE_FILE`:

.. code-block:: console

   export XTENSAD_LICENSE_FILE=84300@xtensa01p.elic.intel.com

Install the prebuilt simulator and ROM
======================================

The simulator team has release version of prebuilt simulators we can
run Zephyr on. The Zephyr team has already customized the available
prebuilt binary package for you. You can download it as an archive from
the same folder as the toolchain. The naming scheme for the archives follow
the template sim_{board}_{date}.tar.bz2. For example,
"sim_ptl_20221018.tar.bz2". The later the date, the more recent the
release. Install the simulator by downloading and extracting the archive:

.. code-block:: console

   tar xvf sim_ptl_20221018.tar.bz2 -C ~/

After the simulator and the ROM are installed, you will need to set the
ACE_SIM_DIR environment variable (yes, even for PTL - this
will be updated soon). To run on another version of the simulator,
export the path to that version. For example:

.. code-block:: console

   export ACE_SIM_DIR=~/sim_ptl_20221018

Building Rimage
###############

You will need to build the upstream rimage and install it:

.. code-block:: console

   git clone https://github.com/thesofproject/rimage
   cd rimage
   git submodule init
   git submodule update
   cmake -B build/
   sudo make -C build/ install

If you get a error message "error: Unsupported config version 3.0",
you might need to get the latest code of rimage tool from upstream and
rebuild it.

Building a Zephyr Application
#############################

The `intel_adsp_ace15_mtpm_sim` board currently exists as patches on the internal
`Zephyr` repository. You can still build for it as you would any other
upstream board:

.. code-block:: console

   west build -p auto -b intel_adsp_ace30_ptl_sim samples/hello_world


Run in the Simulator
####################

Invocation of the simulator itself is somewhat involved, so the
details are now handled by a wrapper script (acesim.py), which is
integrated as a Zephyr native emulator.

The PTL simulator is available from
http://gale.hf.intel.com/~nashif/audio/simulator/. Get
the tarball and extract the contents on your host, then set the ACE_SIM_DIR
environment variable to the path of the directory with the content of the
tarball.

After building with west, call

.. code-block:: console

   ninja -C build run

You can also build and run in one single command:

.. code-block:: console

   west build -p auto -b intel_adsp_ace30_ptl_sim samples/hello_world -t run

This is a typical output for the simulator:

.. code-block:: console

   $ ninja -C build run
   ...
   ACE simulator comm port = 48552+ export XTENSA_CORE=ace30_LX7HiFi4
   + VER=RI-2021.8-linux
   + [ -n /home/laurenmu/xtensa/XtDevTools/install/tools -a -e /home/laurenmu/xtensa/XtDevTools/install/tools/RI-2021.8-linux ]
   + TOOLS=/home/laurenmu/xtensa/XtDevTools/install/tools
   + export TOOLCHAIN_VER=RI-2021.8-linux
   + export XTENSA_TOOLS_VERSION=RI-2021.8-linux
   + dirname /home/laurenmu/xtensa/XtDevTools/install/tools
   + export XTENSA_BUILDS_DIR=/home/laurenmu/xtensa/XtDevTools/install/builds
   + echo PREBUILT: xt-bin-path: /home/laurenmu/xtensa/XtDevTools/install/tools/RI-2021.8-linux/XtensaTools/bin
   + cd /home/laurenmu/sim/sim-ptl-20221018
   + exec ./dsp_fw_sim --platform=mtl --config=/tmp/tmp6myxjr80 --comm_port=48552 --xtsc.turbo=true --xxdebug=0 --xxdebug=1 --xxdebug=2 --SimStaticVectorSelect=1 --AltResetVec=0x3ff80000
   PREBUILT: xt-bin-path: /home/laurenmu/xtensa/XtDevTools/install/tools/RI-2021.8-linux/XtensaTools/bin

         SystemC 2.3.3-Accellera --- Dec 10 2021 23:16:54
         Copyright (c) 1996-2018 by all Contributors,
         ALL RIGHTS RESERVED
   NOTE:        0.0/000: SC_MAIN start, 1.0.0.0 version built Oct 18 2022 at 11:13:03
   NOTE:        0.0/000: setting config for mtl with core ace30_LX7HiFi4
   NOTE:        0.0/000: "fw_bin_file" = /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/zephyr.ri
   NOTE:        0.0/000: "dsp_program" = /home/laurenmu/sim/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex
   NOTE:        0.0/000: XTENSA_TOOLS_VERSION = RI-2021.8-linux, XTENSA_BUILDS = /home/laurenmu/xtensa/XtDevTools/install/builds
   NOTE:        0.0/000: config: ace30_LX7HiFi4, registry: /home/laurenmu/xtensa/XtDevTools/install/builds/RI-2021.8-linux/ace30_LX7HiFi4/config
   NOTE:        0.0/000: "xtensa_registry" = /home/laurenmu/xtensa/XtDevTools/install/builds/RI-2021.8-linux/ace30_LX7HiFi4/config
   NOTE:        0.0/000: dsp program to load: /home/laurenmu/sim/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex
   NOTE    dsp_system      -        0.0/000: Connecting host_fabric to dsp_fabric.
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core0 with following params: core_id: 0, core_type: 0, l1_mmio_name:dram0
   NOTE    dsp_system      -        0.0/000: 0[ms]: loading /home/laurenmu/sim/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex on core 0
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core1 with following params: core_id: 1, core_type: 1, l1_mmio_name:dram0
   NOTE    dsp_system      -        0.0/000: 0[ms]: loading /home/laurenmu/sim/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex on core 1
   NOTE    dsp_system      -        0.0/000: 0[ms]: Creating DSP Core2 with following params: core_id: 2, core_type: 1, l1_mmio_name:dram0
   NOTE    dsp_system      -        0.0/000: 0[ms]: loading /home/laurenmu/sim/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex on core 2
   NOTE    dsp_system      -        0.0/000: Configuring module dsp_mmio.
   NOTE    dsp_system      -        0.0/000: Connecting module dsp_mmio to fabric... Port: 0.
   NOTE    dsp_system      -        0.0/000: Configuring IMR... (delay=360)
   NOTE    dsp_system      -        0.0/000: Connecting IMR to fabric...
   NOTE    dsp_system      -        0.0/000: Connecting HPSRAM to fabric...
   NOTE    dsp_system      -        0.0/000: Configure ulp_l2_sram... (delay=7)
   NOTE    dsp_system      -        0.0/000: Connecting ulp_l2_sram to fabric...
   NOTE    dsp_system      -        0.0/000: Configuring LPSRAM... (delay=7), turbo_lpsram=1
   NOTE    dsp_system      -        0.0/000: Connecting LPSRAM to fabric...
   NOTE    dsp_system      -        0.0/000: Building host...
   NOTE    dsp_system      -        0.0/000: Building host module...
   NOTE    host_module     -        0.0/000: Comm port:48552.
   NOTE    dsp_system      -        0.0/000: Building host module... DONE
   NOTE    dsp_system      -        0.0/000: Creating host mmio...
   NOTE    dsp_system      -        0.0/000: Connect mmio to fabric...
   NOTE    dsp_system      -        0.0/000: Creating host mmio...
   NOTE    dsp_system      -        0.0/000: Connect mmio to fabric...
   NOTE    dsp_system      -        0.0/000: Creating host memory...
   NOTE    dsp_system      -        0.0/000: Connecting memory to fabric...
   NOTE    dsp_system      -        0.0/000: Host memory... DONE
   NOTE    dsp_system      -        0.0/000: Building ace interrupts...
   NOTE    dsp_system      -        0.0/000: Building ace interrupts... DONE
   NOTE    dsp_system      -        0.0/000: FW File loaded into local memory. Copying to IMR to address a1040000, size = 13000
   NOTE    dsp_system      -        0.0/000: Insert auth_api code
   NOTE    dsp_system      -        0.0/000: Building ace controls...
   NOTE    dsp_system      -        0.0/000: Creating alh control...
   NOTE    dsp_system      -        0.0/000: Creating misc(DTF, SETIDVAL) control...
   NOTE    dsp_system      -        0.0/000: Creating comm widget...
   NOTE    dsp_system      -        0.0/000: Creating hda_isd...
   NOTE    dsp_system      -        0.0/000: Creating hda_osd...
   NOTE    dsp_system      -        0.0/000: Creating ssp control...
   NOTE    dsp_system      -        0.0/000: Creating uaol control...
   NOTE    dsp_system      -        0.0/000: Creating soundwire control...
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 0 control...
   NOTE    soundwire_master_0 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 1 control...
   NOTE    soundwire_master_1 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 2 control...
   NOTE    soundwire_master_2 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating soundwire master 3 control...
   NOTE    soundwire_master_3 -        0.0/000: 0[ms]: soundwire_master::soundwire_master()
   NOTE    dsp_system      -        0.0/000: Creating tlb module on HP SRAM ...
   NOTE    dsp_system      -        0.0/000: Connecting TLB to mmio...
   NOTE    dsp_system      -        0.0/000: Connecting tlb module to fabric...
   NOTE    dsp_system      -        0.0/000: Creating hda_dma...
   NOTE    dsp_system      -        0.0/000: Connecting hda_dma to fabric.
   NOTE    dmic_ctrl.hq_inject -        0.0/000: Clock period set to: 8333 ns.
   NOTE    dmic_ctrl.hq_inject -        0.0/000: Basic period: 1 ns.
   NOTE    dmic_ctrl.lp_inject -        0.0/000: Clock period set to: 25 us.
   NOTE    dmic_ctrl.lp_inject -        0.0/000: Basic period: 1 ns.
   NOTE    dmic_ctrl       -        0.0/000: Allocating dmic handshake.
   NOTE    gpdma_0         -        0.0/000: Creating dma: gpdma_0. m_channel_cnt = 8
   NOTE    gpdma_1         -        0.0/000: Creating dma: gpdma_1. m_channel_cnt = 8
   NOTE    gpdma_2         -        0.0/000: Creating dma: gpdma_2. m_channel_cnt = 8
   NOTE    dsp_system      -        0.0/000: Connecting GNA accelerator to dsp fabric.
   NOTE    dp_dma_int_aggr -        0.0/000: dp_gpdma_int_aggr_ace resizing with channels. Current size: 1
   NOTE    dp_gpdma_0      -        0.0/000: Creating dma: dp_gpdma_0. m_channel_cnt = 2
   core0: SOCKET:20000
   NOTE    core0           -        0.0/000: Debug info: port=20000 wait=true ()
   Core 0 active:(start with "(xt-gdb) target remote :20000")
   core1: SOCKET:20001
   NOTE    core1           -        0.0/000: Debug info: port=20001 wait=true ()
   Core 1 active:(start with "(xt-gdb) target remote :20001")
   core2: SOCKET:20002
   NOTE    core2           -        0.0/000: Debug info: port=20002 wait=true ()
   Core 2 active:(start with "(xt-gdb) target remote :20002")
   NOTE    hpsram_memory   -        0.0/000: Thread started.
   NOTE    hpsram_memory   -        0.0/000: Thread started.
   NOTE    lpsram_memory   -        0.0/000: Thread started.
   NOTE    lpsram_memory   -        0.0/000: Thread started.
   NOTE    host_module     -        0.0/000: Main thread started.
   NOTE    host_module     -        0.0/000: Interrupt thread started.
   NOTE    host_module     -        0.0/000: Tick thread started. Period: 400 us.
   NOTE    timer_control   -        0.0/000: Wall Clock Thread started.
   NOTE    ipc_control     -        0.0/000: IPC Control Thread started.
   NOTE    sb_ipc_control  -        0.0/000: IPC Control Thread started.
   NOTE    idc_control     -        0.0/000: IDC Control Thread started.
   NOTE    caps_control    -        0.0/000: GPData Status 1 changed: 0x0
   NOTE    power_control   -        0.0/000: Thread started.
   NOTE    power_control   -        0.0/000: 0[ms]: disable core:1
   NOTE    power_control   -        0.0/000: 0[ms]: disable core:2
   NOTE    hda_controller  -        0.0/000: HD-A Controller Thread started.
   NOTE    comm_widget     -        0.0/000: Comm widget thread started.
   NOTE    hda_dma         -        0.0/000: Thread started.
   NOTE    dmic_ctrl       -        0.0/000: LP channel cnt changed 2 -> 1.
   NOTE    dmic_ctrl       -        0.0/000: HQ sample size changed 2 -> 2.
   NOTE    dmic_ctrl       -        0.0/000: HQ channel cnt changed 2 -> 1.
   NOTE    gpdma_int_aggr  -        0.0/000: Thread started.
   NOTE    gna_accelerator -        0.0/000: GNA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA Hardware Device not available, using Gna2DeviceVersionSoftwareEmulation.
   NOTE    gna_accelerator -        0.0/000: GNA DMA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA compute thread started.
   NOTE    memory_control  -        0.0/000: Thread started.
   NOTE    dp_dma_int_aggr -        0.0/000: Thread started.
   NOTE    caps_control    - 59682807.3/885: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 59682835.3/925: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 59683192.3/013: GPData Status 1 changed: 0x9
   NOTE    power_control   - 64449648.3/004: 161[ms]: enable core:1
   NOTE    caps_control    - 64449693.3/051: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 64449721.3/090: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 64450078.3/182: GPData Status 1 changed: 0x9
   NOTE    power_control   - 64457881.3/773: 161[ms]: enable core:2
   NOTE    caps_control    - 64457926.3/819: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 64457954.3/859: GPData Status 1 changed: 0x8
   NOTE    caps_control    - 64458311.3/953: GPData Status 1 changed: 0x9
   *** Booting Zephyr OS build zephyr-v3.2.0-890-g566484cf9d70  ***
   Hello World! intel_adsp_ace30_ptl_sim

Note that startup is slow, taking ~18 seconds on a tyipcal laptop to reach
Zephyr initialization. Once running, it seems to be 200-400x
slower than the emulated cores. Be patient, especially with code that
busy waits (timers will warp ahead as long as all the cores reach
idle).

By default, there is much output printed to the screen while it works.
You can use "--verbose" to get even more logging from the simulator,
or "--quiet" to suppress everything but the Zephyr logging.

The emulator is configured to use the prebuilt versions of the
ROM, signing key, and simulator (in the simulator directory) and to use
the system rimage (/usr/local/bin/rimage). If you really need to,
you can override these defaults by passing West values for the CMake variables
that will be used as the arguments to the build system and acesim.py.
Check the --help output of acesim.py for the arguments, then look at
cmake/emu/acesim.cmake and boards/xtensa/intel_adsp_ace30_ptl/CMakeLists.txt
in the Zephyr tree for the variables to set. For example:

.. code-block:: console

   west build -p auto -b intel_adsp_ace30_ptl_sim samples/hello_world -t run -- -DSIM_WRAP=$HOME/sim-ptl-20221018 -DRIMAGE=$ZEPHYR_BASE/../modules/audio/sof/rimage/build/rimage -DROM=$HOME/sim-ptl-20221018/bin/dsp_rom_mtl_sim.hex

Finally, note that the wrapper script is written to use the
Ubuntu-provided Python 3.8 in /usr/bin and NOT the half-decade-stale
Anaconda python 3.6 you'll find ahead of it on PATH. Don't try to run
it with "python" on the command line or change the #! line to use
/usr/bin/env.

GDB access
##########

GDB protocol (the Xtensa variant thereof - you must use xt-gdb, an
upstream GNU gdb won't work) debugger access to the cores is provided
by the simulator. At boot, you will see messages emitted that look
like (these can be hard to find in the scrollback, I apologize):

.. code-block:: console

  Core 0 active:(start with "(xt-gdb) target remote :20000")
  Core 1 active:(start with "(xt-gdb) target remote :20001")
  Core 2 active:(start with "(xt-gdb) target remote :20002")

Note that each core is separately managed. There is no gdb
"threading" support provided, so it's not possible to e.g. trap a
breakpoint on any core, etc.

Simply choose the core you want (almost certainly 0, debugging SMP
code this way is extremely difficult) and connect to it in a different
shell on the container:

.. code-block:: console

   xt-gdb build/zepyr/zephyr.elf
   (xt-gdb) target remote :20000

Note that the core will already have started, so you will see it
stopped in an arbitrary state, likely in the idle thread. This
probably isn't what you want, so acesim.py provides a
-d/--start-halted option that suppresses the automatic start of the
DSP cores.

Now when gdb connects, the emulated core 0 is halted at the hardware
reset address 0x3ff80000. You can start the simulator with a
"continue" command, set breakpoints first, etc.

Note that the ROM addresses are part of the ROM binary and not Zephyr,
so the symbol table for early boot will not be available in the
debugger.  As long as the ROM does its job and hands off to Zephyr,
you will be in a safe environment with symbols after a few dozen
instructions.  If you do need to debug the ROM, you can specify it's
ELF file on the command line instead, or use the gdb "file" command to
change the symbol table.

Troubleshooting
################

Here are some possible failures you might encounter for reference:

- Cannot find the C complier

This error can happen as a result of license server issues. You can export
FLEXLM_DIAGNOSTICS=3 to get the detailed server connection log. The incorrect
machine time will cause failure. If the connection still fails, you can try to
clear your caches by deleting the ~/.cache and ~/.ccache directories.

- Cannot find simulator

When West can't find the simulator on the path you gave it, you'll get an error like this:

.. code-block:: console

   ...
   Firmware manifest and signing completed !
   [2/3] cd /home/laurenmu/intel-zephyrproject/zephyr/build && ACESIM-NOTFOUND --rom --sim --rimage /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/zephyr.ri
   /bin/sh: 1: ACESIM-NOTFOUND: not found
   FAILED: zephyr/CMakeFiles/run_acesim /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/CMakeFiles/run_acesim
   cd /home/laurenmu/intel-zephyrproject/zephyr/build && ACESIM-NOTFOUND --rom --sim --rimage /home/laurenmu/intel-zephyrproject/zephyr/build/zephyr/zephyr.ri
   ninja: build stopped: subcommand failed.
   FATAL ERROR: command exited with status 1: /usr/bin/cmake --build /home/laurenmu/intel-zephyrproject/zephyr/build --target run

If you've set the path but still get this error, rebuild Zephyr with the
pristine option -p. The Linux environment variable is copied into CMake during
the configuration stage, so if you set or change the environment variable without
redoing the build from scratch, it won't know where the simulator is.

- No Zephyr output message

Sometimes the simulator runs, but hangs after the message "Thread started"
For example:

.. code-block:: console

   NOTE    gna_accelerator -        0.0/000: GNA DMA thread started.
   NOTE    gna_accelerator -        0.0/000: GNA compute thread started.
   NOTE    memory_control  -        0.0/000: Thread started.
   NOTE    dp_dma_int_aggr -        0.0/000: Thread started.

One of the possible reason is the xt-gdb failed to start. You can run
$XTENSA_TOOLCHAIN_PATH/$TOOLCHAIN_VER/XtensaTools/bin/xt-gdb in the
terminal to check. It is likely that the environment variable is
not setting correctly or some shared library using by xt-gdb is missing.

- Zephyr cache issues

Clearing your cache is good for more than just license server issues.
If you're having inexplicable errors, try rebuilding after deleting the
caches:

.. code-block:: console

   rm -rf ~/.ccache ~/.cache/zephyr
