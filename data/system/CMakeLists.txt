# What hides the system's own partitions (This PC): a udev rule and the
# helper it calls.
install(FILES 90-raye-hide-system.rules DESTINATION lib/udev/rules.d)
install(PROGRAMS raye-is-system-dm DESTINATION lib/raye/udev)

# No /.hidden: every folder of the Raye drive stays visible (owner's choice).
