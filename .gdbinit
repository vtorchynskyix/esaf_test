define hook-quit
    set confirm off
end

set pagination off

file build/zhalo.elf
set remote exec-file build/zhalo.bin
target extended-remote localhost:4242
# watch state.code


# dprintf run_arm,"at run_arm code is %d\n",state.code
# dprintf on_fc_timeout,"on_fc_timeout\n"

echo \n
echo to reflash, reload, use Ctrl-C and command 'bang'\n
echo \n

define bang
  echo flashing, reloading...
  load
  continue
end

bang
