# OSX Vagrant boxes

There is an `osxsierra` Vagrant box defined, but it has some restrictions on use.
_Only_ if the underlying hardware is detected as Apple Mac and you've checked the licensing agreement is valid and explictly acknowledged this by touching a file.

The Vagrantfile will not allow launching the `osxsierra` box until you do.

Please read [the licensing agreement](http://images.apple.com/legal/sla/docs/macOS1012.pdf).

# #protips

* Getting the PCP build log from the guest.  First install this plugin:


    # will allow you to scp a file from the guest
    # very useful to grab the PCP build log
    vagrant plugin install vagrant-scp

  then you can grab the build log from the guest via:

     vagrant scp osxsierra:/vagrant/Logs/pcp .

# Caveats/Known Issues

* The first time you run `vagrant up osxsierra` you will get an `rsync` error. This can be safely ignored for now, just run `vagrant provision osxsierra` to continue past this.  Honestly the `rsync` error is probably a real issue, but there's more issues to battle through first..

# TODO
* During the `Makepkg` build of PCP there are errors occuring around `tar` failures.
