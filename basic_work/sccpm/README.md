## To download and patch CP/M ##

1. download from Grant Searle's website
2. run smbaker's cpm patches in ../patches/

## To setup CP/M ##

### Phase 1, format the compactflash card ###

    Boot to monitor ROM, press space to get the prompt
    paste form64.hex
    G8100

### Phase 2, install CPM to the compactflash card ###

    Boot to monitor ROM, press space to get the prompt
    paste cpm22.hex
    paste cbios64.hex
    paste putsys.hex
    G8100

### Phase 3, install DOWNLOAD.COM 

    Boot to monitor ROM, press space to get the prompt
    X to boot CP/M, and Y to confirm
    You will see A> prompt
    Boot to monitor ROM, press space to get the prompt
    paste down8100.hex
    GFFE8
    SAVE 2 DOWNLOAD.COM
