# SafeCTRTransfer
Safe, simple, user-friendly CTRNAND transfer tool

SafeCTRTransfer allows you to do the CTRNAND transfer on a previously unhacked 3DS. For CTRNAND transfers on consoles that have arm9loaderhax installed, use [GodMode9](https://github.com/d0k3/GodMode9).

## Quick Instructions
Put the two files listed below into the `sd:/ctrtransfer` folder, then run the SafeCTRTransfer payload (`SafeCTRTransfer.bin`) via the entrypoint of your choice (most likely [safehax](https://github.com/TiniVi/safehax)). Keep in mind that the CTRNAND image will be modified in the process.
* **794,624,000 byte O3DS FW 2.1 CTRNAND image** (with MBR, called `ctrnand_full.bin` in [GodMode9](https://github.com/d0k3/GodMode9)) - this will be modified in the process
* **32 byte SHA file** (filename: same as above with `.sha`appended) - SHA256 checksum, for bigger files anything above 32 byte will be ignored

For extended usage instructions, refer to [Plailect's guide](https://3ds.guide/).

## Credits
* **Normmatt**, for sdmmc.c / sdmmc.h
* **Cha(N)**, **Kane49**, and all other FatFS contributors for FatFS
* **Al3x_10m** for being the first fearless person to test this
* **Plailect** for providing the guide and making this accessible to the common user
* **YoshiB** for providing the splash screen graphics
* **Shadowhand** for being awesome and [hosting my nightlies](https://d0k3.secretalgorithm.com/)
* Everyone involved in the development of **[Decrypt9](https://github.com/d0k3/Decrypt9WIP)** and **[GodMode9](https://github.com/d0k3/GodMode9)**
* The fine folks on **freenode #Cakey**
* All **[3dbrew.org](https://www.3dbrew.org/wiki/Main_Page) editors**
* Everyone I possibly forgot, if you think you deserve to be mentioned, just contact me!
