#!/usr/bin/perl

use strict;
use diagnostics;
use warnings;
use File::Path;
use Net::FTP;
use File::Copy qw(copy);
use List::Util 'first';  

use Cwd;


#=================================================================================================
package CfgInfo;

sub new{
    my ($ref,$cfgname) = @_;  
    my $class = ref($ref) || $ref;  
    my $self = {};  

    my $curdir              = "."; #getcwd();

    $self->{name}           = $cfgname ;

    $self->{product}        = "$curdir/Product/";
    $self->{cfgroot}        = "$self->{product}/$self->{name}config";
    $self->{sysrpmlist}     = "$self->{cfgroot}/cfg/rpms.txt";
    $self->{localrpmlist}   = "$self->{cfgroot}/cfg/rpms_local.txt";
    $self->{ksconf}         = "$self->{cfgroot}/cfg/ks.conf";
    $self->{isobootconf}    = "$self->{cfgroot}/cfg/isolinux.cfg";
    $self->{compsconf}      = "$self->{cfgroot}/cfg/comps.xml";
    $self->{liveimg}        = "$self->{cfgroot}/cfg/squashfs.img";
    $self->{upgradesh}      = "$self->{cfgroot}/cfg/upgrade.sh";
    $self->{anaconda}       = "$self->{cfgroot}/anaconda";

    $self->{localpkgs}      = "$curdir/localpkgs";
    $self->{migration}      = "$curdir/migration";
    $self->{pubanaconda}    = "$self->{product}/anaconda";
    $self->{pubconfig}      = "$self->{product}/pubconfig";
    $self->{template}       = "$self->{product}/template64";
    $self->{tmp}            = "$curdir/tmp";

    bless($self, $class);  

    return $self;  
}

#=================================================================================================
#   function list:
#       UtilCtl::setverinfo      $filename $version    #   replace string,support PCRE
#       UtilCtl::ver2str         version
#       UtilCtl::recreatedir     dirname
package UtilCtl;


sub replace_strings{
    my  ($file,$orig,$new) = @_;
    if( open(my $fh,"$file") ){
        my @lines = <$fh>;
        close $fh;

        if( open(my $fh,">$file") ){
            foreach my $line (@lines){
                $line=~s/$orig/$new/g;
                print $fh $line;
            }
            close $fh;

            return 0;
        }
    }

    return 1;
}
sub replace_version{
    my  ($file,$new) = @_;
    if( open(my $fh,"$file") ){
        my @lines = <$fh>;
        close $fh;

        if( open(my $fh,">$file") ){
            foreach my $line (@lines){
                $line=~s/6\.0\.\d+/$new/g;
                print $fh $line;
            }
            close $fh;

            return 0;
        }
    }

    return 1;
}
sub setverinfo{
    my  ($file,$version) = @_;
    my ($version_major,$version_minor,$version_number) = split(/\./,$version,3);
    replace_strings($file,"TM_VERSTR","$version_major.$version_minor");
    replace_strings($file,"6.0.1111",$version);
}

sub ver2array{
    my ($version) = @_;
    my ($version_major,$version_minor,$version_number) = split(/\./,$version,3);
    return ("$version_major.$version_minor","$version_number");
}

sub ver2str{
    my ($version) = @_;
    my ($version_major,$version_minor,$version_number) = split(/\./,$version,3);
    return "$version_major.$version_minor-$version_number";
}

sub chgvernum{
    my ($version,$number) = @_;
    my ($version_major,$version_minor,$version_number) = split(/\./,$version,3);
    return "$version_major.$version_minor.$number";
}

sub recreatedir{
    my ($dst) = @_;
    qx(env rm -rf $dst);
    qx(env mkdir -p $dst);
}





#=================================================================================================
package FtpCtl;

sub new{
    my ($ref,$host,$usr,$pass,$parent) = @_;  
    my $class = ref($ref) || $ref;  
    my $self = {};  

    $self->{handle} = Net::FTP->new($host) or die "Can't open $host\n";
    $self->{handle}->login($usr, $pass) or die "Can't log $usr in\n";
    $self->{handle}->cwd($parent) or die "Change to $parent fail\n";
    $self->{handle}->binary();

    bless($self, $class);  

    return $self;  
}

sub DESTROY {
    my $self = shift;  

    if( $self->{handle} ){
        $self->{handle}->quit();
    }
}

sub downver{
    my ($self,$name,@remoteinfo) = @_;

    my $parentdir = $self->{handle}->pwd();

    my @dirs = $self->{handle}->ls($name) or die "get $name version list fail\n";
    my $maxdir ;
    print "$name: @dirs\n";
    foreach my $item (@dirs){
        next if($item!~m/^\d+$/);
        $maxdir = $item if( !$maxdir || $item > $maxdir);
    }

    die "No version for $name in $parentdir\n" if( !$maxdir );
    $self->{maxver} = $maxdir;

    while( @remoteinfo ){
        my $item  = shift @remoteinfo;
        my $regrex = shift @remoteinfo;
        my $path = "$parentdir/$name/$maxdir/$item";
        $self->{handle}->cwd($path) or die "Change to $path fail\n";
        my @files =  $self->{handle}->ls($regrex);
        foreach my $file (@files){
            $self->{handle}->get($file) or die "get failed ", $self->{handle}->message;
            print "  >> $path   -   $file\n";
        }
    }
    $self->{handle}->cwd($parentdir) or die "Change to $parentdir fail\n";
}





#=================================================================================================
package RpmCtl;


sub torpmlist{
    my ($fname) = (@_);
    open my $fh,"$fname" or die "conv pkg list $fname open fail\n";
    my @pkgs=<$fh>;
    close $fh;

    chomp(@pkgs);
    s/\.rpm$//g for @pkgs;
    s/(.*\/)?(.*?)((-[0-9][^-]*)+)(\.[^.]+)$/$2$5/g for @pkgs;

    open $fh,">$fname" or die "conv pkg list $fname write fail\n";
    foreach my $line (@pkgs){
        next if($line=~m/^mysql57-community-release/);
        next if($line=~m/^epel-release/);
        print $fh "$line\n";
    }
    close $fh;
};

sub getrpms{
    my ($pkgpath,$pkgname) = @_;
    my $fname;
    my $tail;
    if( $pkgname!~m/(.*)(\.[^.]+)$/){
        die "pkg $pkgname unknown\n" if($pkgname!~m/^gpg-pubkey/);
        $fname=$pkgname;
        $tail='.rpm';
    }
    else{
        $fname=$1;
        $tail="$2.rpm";
    }
    return glob("$pkgpath/$fname-[0-9]*$tail");
}

sub cp2dst{
    my ($srcpath,$dstpath,$pkglist) = @_;

    if( open(my $fh,"$pkglist") ){
        my @localpkgs = <$fh>;
        close $fh;
        chomp(@localpkgs);

        foreach my $line (@localpkgs){
            my @localfile = getrpms($srcpath,$line);
            foreach my $file (@localfile){
                File::Copy::copy($file, $dstpath);
            }
        }
    }
}


sub repack_rpm{
    my  ($outdir,$srcfile,$specfile,$version) = @_;

    my  ($releasever,$releasenum) = UtilCtl::ver2array($version);
    my  $builddir="`pwd`/tmp/rpmbuild";

    if ( ! -f $srcfile || ! -f $specfile ){
        die "file $srcfile or $specfile not found, pack fail\n";
    }
    UtilCtl::recreatedir($builddir);
    UtilCtl::recreatedir("$builddir/SOURCES");
    qx(env cp -a $srcfile "$builddir/SOURCES/");
    qx(env rpmbuild --define "_topdir $builddir"  --define "release_num $releasenum" --define "release_ver $releasever" -bb $specfile);
    qx(env find $builddir/ -name "*.rpm" -exec cp -rf {} $outdir \\;);
}


sub testinstall{
    my ($cfgroot) = (@_);
    my $rpms=qx(env awk '/%packages/,/%end/' $cfgroot/cfg/ks.conf | egrep '^[a-zA-Z]' | xargs);
    $rpms="$rpms kernel-devel" if ($cfgroot!~m/rain/);
    print "1. please upload this files to centos7\n";
    print "     scp rainconfig/cfg/mysql57-community-release-el7-11.noarch.rpm centos7_host\n" if ($cfgroot=~m/rain/);
    print "     scp upgrade.tar.bz2 centos7host\n";
    print "2. please run command in centos7:\n";
    print "     tar xjf upgrade.tar.bz2\n";
    print "     yum install -y epel-release\n";
    print "     yum install -y mysql57-community-release-el7-11.noarch.rpm \n" if ($cfgroot=~m/rain/);
    print "     yum install -y $rpms\n";
    print "     cd migration \n";
    if($cfgroot=~m/rain/){
        print "     rm -f libvirt-daemon-lxc-* libvirt-devel-* libvirt-docs-* libvirt-lock-sanlock-*  *.src.rpm  libvirt-debuginfo-* \n";
    }else{
        print "     rm -f pip_packages* \n";
    }
    print "     yum install -y *.rpm\n";
    print "     rpm -qa > rpms.txt\n";
    print "     scp rpms.txt   to_openvm7/$cfgroot/cfg/\n";
}








#=================================================================================================
package IsoCtl;

sub new{
    my ($ref,$version,$cfgname,$tmp) = @_;  
    my $class = ref($ref) || $ref;  
    my $self = {};  

    $self->{cfg}            = new CfgInfo($cfgname);
    $self->{version}        = $version;

    my $sysrepodir          = "/home/jenkinsbuild/ci-jenkins/workspace/TMrain/centos7repo/";
    #   for local debug, if not build in ci machine, set to /builddir/600src/centos7repo/
    $sysrepodir             = "/builddir/600src/centos7repo/" if (! -d '/home/jenkinsbuild/ci-jenkins/workspace/TMrain/');

    $self->{sysrepo}        = $sysrepodir;

    bless($self, $class);  

    return $self;  
}

sub mnt_mount{
    my ($self,$mntfile,$dstpoint,%cpops) = @_;
    die "file $mntfile not found\n" if (! -f $mntfile);
    qx( env umount -lf $dstpoint 2>/dev/null);
    UtilCtl::recreatedir($dstpoint);
    qx( env mount $mntfile $dstpoint);
    die " mount $mntfile failed \n" if($? != 0 );

    while( my ($src,$dst) = each (%cpops)){
        print " mount exec $src => $dst\n";

        if( $src eq 'f' ){
            next if( -f $dst);

            $self->mnt_umount($dstpoint);
            die "file $dst not found, maybe mount a wrong file $mntfile\n";
        }

        if( $src eq 'd' ){
            next if( -d $dst);

            $self->mnt_umount($dstpoint);
            die "dir $dst not found, maybe mount a wrong file $mntfile\n";
        }

        if( (! -f $src) && (!-d $src) ){
            $self->mnt_umount($dstpoint);
            die "file or dir $src not found, maybe mount a wrong file $mntfile\n";
        }
        qx( env rm -rf $dst);
        qx( env cp -a $src $dst);
    }
}

sub mnt_umount{
    my ($self,$dst) = @_;
    qx( env umount -lf $dst);
    qx( env rm -rf $dst);
}


sub updsysrepo{
    my ($self,$pkgname) = @_;

    File::Path::mkpath($self->{sysrepo},1,0755);
    die "path $self->{sysrepo} not exist\n" if( ! -d $self->{sysrepo} );

    my $fh ;
    my @requestpkgs;

    if( !$pkgname ){
        open($fh,"$self->{cfg}->{sysrpmlist}") or die "open file $self->{cfg}->{sysrpmlist} to down pkgs fail\n";
        chomp(@requestpkgs = <$fh>);
        close $fh;
    }
    else{
        chomp(@requestpkgs=split(/,/,$pkgname));
    }

    if( open($fh,"$self->{cfg}->{localrpmlist}") ){
        chomp( my @localpkgs = <$fh>);
        close $fh;

        my @tmppkgs = @requestpkgs;
        my %tmphash;
        @tmphash{ @tmppkgs } = ();
        delete @tmphash{ @localpkgs };
        @requestpkgs = keys %tmphash;
    }

    #   call yumdownlaod to down pkgs from centos repo
    foreach my $line (@requestpkgs){
        chomp($line);
        next if($line!~m/\w/);
        next if($line=~m/^gpg-pubkey/);

        my @files = RpmCtl::getrpms($self->{sysrepo},$line);
        next if( scalar(@files) > 0 );

        qx(env yumdownloader $line --downloadonly --destdir $self->{sysrepo});
        @files = RpmCtl::getrpms($self->{sysrepo},$line);
        print "glob  $line result: @files\n";
        die "down file $line fail\n" if( scalar(@files) < 1 );
    }

    #   create localrepo
    qx(env createrepo $self->{sysrepo});
    print "\e[41m*** please use:\n   yum update -y \nto update local build toolchain \e[0m*** \n";
}

sub buildiso{
    my ($self,$outputdir,$isodir,$version) = @_;
    die "dir $outputdir not exist\n" if( ! -d $outputdir );
    die "check dir $isodir fail\n" if( !-d $isodir);

    if($version){
        UtilCtl::replace_version("$isodir/isolinux/isolinux.cfg",$version);
    }
    else{
        $version    = $self->{version};
    }
    my $ext         = ($self->{cfg}->{name} eq 'sa') ?'SA':'';
    my $label       = "TMrain${ext}_$version";
    my $verstr      = UtilCtl::ver2str($version);
    my $outputfile  = "$outputdir/TMrain${ext}-$verstr.iso";
    my $geniso_param="-untranslated-filenames -V $label -J -joliet-long ";

    $geniso_param .= '-rational-rock -translation-table -input-charset utf-8 ';
    $geniso_param .= '-x ./lost+found -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot ';
    $geniso_param .= '-boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot ';

    unlink $outputfile;
    print "\n================= BEGIN GEN ISO FILE $outputfile ====================\n";

    my $curdir = File::Path::getcwd();
    chdir ($isodir);
    qx(env createrepo  -g  repodata/comps.xml ./);
    chdir($curdir);

    qx(env genisoimage  $geniso_param -o $outputfile -T $isodir);
    die "gen iso file $outputfile fail\n" if( !-f $outputfile);
    qx(env checkisomd5 $outputfile);
    print "Finish gen iso file $outputfile\n";
}

sub initiso{
    my ($self,$isodir) = @_;
    my $ext = ($self->{cfg}->{name} eq 'sa') ?'SA':'';

    die "centos 7 template not found\n"                             if( ! -d $self->{cfg}->{template});
    die "$self->{cfg}->name} packages dir($self->{cfg}->{localpkgs} not found\n"   if( ! -d $self->{cfg}->{localpkgs});
    die "missing package: \n\tanaconda createrepo mkisofs rsync yum-utils genisoimage squashfs-tools\n" 
    if( !-f "/usr/bin/yumdownloader"
        || ! -f "/usr/bin/rsync" || !-f "/usr/bin/createrepo" || !-f "/usr/bin/genisoimage" || !-f "/usr/bin/checkisomd5");

    UtilCtl::recreatedir($isodir);

    die "$self->{cfg}->{name} packages dir($self->{cfg}->{cfgroot}) not found\n"   if( ! -d $self->{cfg}->{cfgroot});

    print "\n================= BEGIN PREPARE ISO DIR $isodir ====================\n";
    qx(env rsync -a --exclude=Packages/ --exclude=repodata/ $self->{cfg}->{template}/ $isodir);
    mkdir "$isodir/Packages";
    mkdir "$isodir/repodata";
    RpmCtl::cp2dst($self->{sysrepo},"$isodir/Packages",$self->{cfg}->{sysrpmlist});
    RpmCtl::cp2dst($self->{cfg}->{localpkgs},"$isodir/Packages",$self->{cfg}->{localrpmlist});
    unlink("$isodir/LiveOS/squashfs.img");
    unlink("$isodir/isolinux/isolinux.cfg");
    qx(env cp -a $self->{cfg}->{liveimg}       $isodir/LiveOS/);
    qx(env cp -a $self->{cfg}->{ksconf}        $isodir/isolinux/);
    qx(env cp -a $self->{cfg}->{isobootconf}   $isodir/isolinux/);
    die "$isodir/isolinux/isolinux.cfg not found\n" if ( ! -f "$isodir/isolinux/isolinux.cfg");
    UtilCtl::setverinfo("$isodir/isolinux/ks.conf","6.0.1111",$self->{version});
    UtilCtl::setverinfo("$isodir/isolinux/isolinux.cfg",$self->{version});
    
    chown(0,0, "$isodir/isolinux/isolinux.cfg");
    chmod(0644,"$isodir/isolinux/isolinux.cfg");

    #if( -d "$basedir/rain" ){
    #    qx(env cp -a $basedir/rain $isodir);
    #}
    qx(env cp -a $self->{cfg}->{compsconf}      $isodir/repodata/);
    die "$isodir/repodata/comps.xml not found\n" if ( ! -f "$isodir/repodata/comps.xml");
}


#   update base iso image, copy latest image files to $dstbase dir
sub settemplate{
    my ($self,$isofile) = @_;
    UtilCtl::recreatedir($self->{cfg}->{template});

    my $isopoint = "$self->{cfg}->{tmp}/newiso/";
    $self->mnt_mount($isofile,$isopoint,"f","$self->{cfg}->{tmp}/newiso/LiveOS/squashfs.img");

    print "begin update $self->{cfg}->{template}\n";
    qx(env rsync -a --exclude=Packages/ --exclude=repodata/ $isopoint  $self->{cfg}->{template}); 
    $self->mnt_umount($isopoint);
    die "sync fail,dst file $self->{cfg}->{template}/LiveOS/squashfs.img not found\n" if ( ! -f "$self->{cfg}->{template}/LiveOS/squashfs.img" );

    print "begin update anaconda files\n";
    my $newroot="$self->{cfg}->{tmp}/newroot";
    my $pysrc="$newroot/usr/lib64/python2.7/site-packages/pyanaconda";
    my $pydst="$self->{anaconda}/pyanaconda";
    my $uisrc="$newroot/usr/share/anaconda/ui";
    my $uidst="$self->{anaconda}/ui";

    UtilCtl::recreatedir($self->{anaconda});
    $self->mnt_mount("$self->{cfg}->{template}/LiveOS/squashfs.img","$self->{cfg}->{tmp}/newsq","f","$self->{cfg}->{tmp}/newsq/LiveOS/rootfs.img");
    $self->mnt_mount("$self->{cfg}->{tmp}/newsq/LiveOS/rootfs.img",$newroot,
        $pysrc,$pydst,$uisrc,$uidst);
    $self->mnt_umount($newroot);
    $self->mnt_umount("$self->{cfg}->{tmp}/newsq");

    print "begin create custom anaconda files\n";
    qx(env cp -a $uidst/spokes/password.glade $uidst/spokes/rainpassword.glade);

    UtilCtl::replace_strings("$uidst/spokes/rainpassword.glade","ROOT","TMrain");
    UtilCtl::replace_strings("$uidst/spokes/rainpassword.glade","root","tmrain");
    UtilCtl::replace_strings("$uidst/spokes/rainpassword.glade","id=\"","id=\"tmrain");

    qx(env mv $self->{cfg}->{product}/rainconfig/anaconda $self->{cfg}->{product}/rainconfig/anaconda_old );
    qx(env mv $self->{cfg}->{product}/saconfig/anaconda  $self->{cfg}->{product}/saconfig/anaconda_old );
    qx(env mv $self->{cfg}->{product}/rainconfig/cfg/squashfs.img $self->{cfg}->{product}/rainconfig/cfg/squashfs.img_old );
    qx(env mv $self->{cfg}->{product}/saconfig/cfg/squashfs.img  $self->{cfg}->{product}/saconfig/cfg/squashfs.img_old );
    qx(env cp -a $self->{anaconda} $self->{product}/rainconfig/);
    qx(env cp -a $self->{anaconda} $self->{product}/saconfig/);
    qx(env cp -a $self->{cfg}->{template}/LiveOS/squashfs.img $self->{cfg}->{product}/rainconfig/cfg/);
    qx(env cp -a $self->{cfg}->{template}/LiveOS/squashfs.img $self->{cfg}->{product}/saconfig/cfg/);
    print "Finish update iso base to $isofile\n";
    print "\e[41m*** please use:\n   yum update -y \nto update local build toolchain \e[0m*** \n";
}

sub updbootfs{
    my  ($self)     = @_;
    my $outpoint    = "$self->{cfg}->{tmp}/newinst";
    my $squashpoint = "$self->{cfg}->{tmp}/squash";
    my $mountpoint  = "$self->{cfg}->{tmp}/squashtmp";
    my $squashfsimg = $self->{cfg}->{liveimg};

    #   check if squashfs-tools exists
    my $cmd = qx{whereis mksquashfs};
    die "cmd  mksquashfs not found, terminate\n" if( $cmd !~m/\/\w?bin\/mksquashfs/);

    print "prepare squashfs\n";
    UtilCtl::recreatedir($squashpoint);
    $self->mnt_mount($squashfsimg,$mountpoint,"f","$mountpoint/LiveOS/rootfs.img",
        $mountpoint,$squashpoint);
    $self->mnt_umount($mountpoint);

    my $pysrc = "$self->{cfg}->{cfgroot}/anaconda/pyanaconda";
    my $pydst = "$outpoint/usr/lib64/python2.7/site-packages/pyanaconda";
    my $uisrc = "$self->{cfg}->{cfgroot}/anaconda/ui/spokes";
    my $uidst = "$outpoint/usr/share/anaconda/ui/spokes";
    print "prepare mount install image to $outpoint and update files\n";
    $self->mnt_mount("$squashpoint/LiveOS/rootfs.img",$outpoint,
        $pysrc,$pydst,$uisrc,$uidst
    );
    qx(env chown -R root.root $pydst);
    qx(env chown -R root.root $uidst);
    qx(env rm -f $outpoint/usr/share/centos-release/EULA.Trendmicro);
    qx(env rm -f $outpoint/usr/share/centos-release/EULA.Trendmicro);
    qx(env cp -a $self->{cfg}->{cfgroot}/cfg/EULA.Trendmicro $outpoint/usr/share/centos-release/EULA.Trendmicro);
    $self->mnt_umount($outpoint);

    qx(env rm -f $squashfsimg);
    qx(env mksquashfs $squashpoint $squashfsimg);
    print "Finish repace install image to $squashfsimg\n";
}



#=================================================================================================
package rainCtl;

sub new{
    my ($ref,$version,$cfgname) = @_;  
    my $class = ref($ref) || $ref;  
    my $self = {};  

    my $curdir              = "."; #getcwd();

    $self->{cfg}            = new CfgInfo($cfgname);

    $self->{version}        = $version;
    $self->{uniaver}        = $version;

    bless($self, $class);  

    return $self;  
}


sub genupgrade{
    my ($self,$outputdir) = @_;
    my $verstr = UtilCtl::ver2str($self->{version});
    my $outputfile = "$outputdir/upgrade-$verstr.tar.bz2";
    my $workdir = $self->{cfg}->{migration};
    print "\n================= BEGIN GEN UPGRADE FILE $outputfile ====================\n";
    qx(env rm -rf $outputdir/upgrade-* );
    UtilCtl::recreatedir($workdir);
    RpmCtl::cp2dst($self->{cfg}->{localpkgs},$workdir,$self->{cfg}->{localrpmlist});
    qx(env cp -a $self->{cfg}->{upgradesh} $workdir/ );
    UtilCtl::setverinfo("$workdir/upgrade.sh",$self->{version});

    qx(env tar cjf $outputfile $workdir/upgrade.sh $workdir/*.rpm );
    print "Finish gen upgrade file $outputfile\n";
}


sub reget_rainfiles{
    my ($self,$ftpinfo,@files) = @_;
    
    UtilCtl::recreatedir($self->{cfg}->{localpkgs});
    qx(env cp $self->{cfg}->{cfgroot}/pkgs/* $self->{cfg}->{localpkgs}) if( -d "$self->{cfg}->{cfgroot}/pkgs/" );
    qx(env cp $self->{cfg}->{pubconfig}/pkgs/*.rpm $self->{cfg}->{localpkgs}) if ( -d "$self->{cfg}->{pubconfig}/pkgs/" );

    return if( !$ftpinfo );

    print "\n================= BEGIN GET LATEST rain PACKAGES ====================\n";
    my $localolddir  = File::Path::getcwd();
    chdir $self->{cfg}->{localpkgs} or die "local change to $self->{cfg}->{localpkgs} fail\n";
    my $ftp = new FtpCtl($ftpinfo->{host},$ftpinfo->{usr},$ftpinfo->{pass},$ftpinfo->{parent});
    foreach my $item  (@files){
        $ftp->downver(@$item);
        $self->{uniaver} = $ftp->{maxver} if( $$item[0] =~m/unia/i);
    }
    #undef $ftp;
    chdir $localolddir;
}


sub regen_rainpkgs{
    my  ($self) = @_;

    print "\n================= BEGIN UPDATE LATEST rain PACKAGES ====================\n";
    #   repack clish_cmd
    qx(env rm -f $self->{cfg}->{localpkgs}/clish_cmd.tar.gz);
    qx(env tar czf $self->{cfg}->{localpkgs}/clish_cmd.tar.gz -C $self->{cfg}->{cfgroot} cli/);
    qx(rm -f $self->{cfg}->{localpkgs}/clish_cmd*.rpm);
    RpmCtl::repack_rpm($self->{cfg}->{localpkgs},"$self->{cfg}->{localpkgs}/clish_cmd.tar.gz","$self->{cfg}->{cfgroot}/cfg/clish_cmd.spec",$self->{version});

    #   createe unia rpm
    if( -f "$self->{cfg}->{localpkgs}/unia.tar.gz" ){
        my $Uniaver = UtilCtl::chgvernum($self->{version},$self->{uniaver});
        print "repack unia $self->{uniaver}\n";
        qx(rm -f $self->{cfg}->{localpkgs}/unia*.rpm);
        RpmCtl::repack_rpm($self->{cfg}->{localpkgs},"$self->{cfg}->{localpkgs}/unia.tar.gz","$self->{cfg}->{cfgroot}/cfg/unia.spec",$Uniaver);
    }
    else{
        print "unia package not found, skip repack\n" if ($self->{cfg}->{cfgroot} =~m/rainconfig/);
    }
}


#=================================================================================================
package CmdCtl;



sub new{
    my ($ref,$cfgname,$isus,$version,@args) = @_;  
    my $class = ref($ref) || $ref;  
    my $self = {};  


    $self->{args}           = \@args;
    $self->{cfg}            = new CfgInfo($cfgname);

    $self->{version}        = $version;

    my %ftpinfo             = (
        host => '10.204.16.2',
        usr  => 'rain',
        pass => 'rain',
        parent => 'rain',
        rainfiles => [
            # module dir        sub dir1           file regrex1,  can include many sub dir and file regrex pair
            ['Kernel',          'mui/output/RPMS',"kernel-*.rpm" ],
            ['Libvirt',         'mui/output/RPMS',"libvirt-*.rpm" ],
        ],
        safiles => [
            ['SA',              'mui/output/', "rain_gateway*.rpm"],
        ]
    );

    $ftpinfo{parent} = "$ftpinfo{parent}-us" if( $isus);


    $self->{ftpinfo}        = \%ftpinfo;
    $self->{ftpfiles}       = \@{$ftpinfo{"${cfgname}files"}} ;

    $self->{hiso}           = new IsoCtl($version,$cfgname);
    $self->{hrain}           = new rainCtl($version,$cfgname);

    bless($self, $class);  

    return $self;  
}

sub exec{
    my ($self) = @_;

    $self->help() if( grep /^(help|h|-h|--help|\?)$/i,@{$self->{args}} );

    my $updrepo = List::Util::first {/^updrepo/} @{$self->{args}};
    if( $updrepo ){
        my $pkgname;
        $pkgname=$1 if ( $updrepo =~m/^updrepo=(.*)/);
        $self->{hiso}->updsysrepo($pkgname);
        print "finish download centos pkgs\n";                
        exit;
    }

    if( grep /^updlive$/,@{$self->{args}}  ){
        $self->{hiso}->updbootfs();

        print "finish repack exit \n";
        exit;
    }

    if( grep /^testinst$/,@{$self->{args}}  ){
        RpmCtl::testinstall($self->{cfg}->{cfgroot});
        exit;
    }

    #   updlist cmd:
    #       update rpms.txt and rpms_local.txt
    if( grep /^updlist$/,@{$self->{args}}  ){
        RpmCtl::torpmlist("$self->{cfg}->{sysrpmlist}");
        RpmCtl::torpmlist("$self->{cfg}->{localrpmlist}");
        print "finish update $self->{cfg}->{cfgroot} 's rpms.txt and rpms_local.txt\n";
        exit;
    }

    my $repackiso = List::Util::first {/^repackiso/} @{$self->{args}};
    if( $repackiso ){
        my $isodir="$self->{cfg}->{tmp}/iso/";
        my $version;
        if ( $repackiso =~m/^repackiso=(.*)/){
            my $line=$1;
            my @info=split /,/,$line;
            $isodir=shift @info;
            $version=shift @info;
        }
        $self->{hiso}->buildiso("./",$isodir,$version);
        exit 0;
    }

    qx( env rm -rf $self->{cfg}->{tmp});

    undef $self->{ftpinfo} if(grep /^noftp$/,@{$self->{args}});

    $self->{hrain}->reget_rainfiles($self->{ftpinfo},@{$self->{ftpfiles}});
    $self->{hrain}->regen_rainpkgs();

    $self->{hiso}->initiso("$self->{cfg}->{tmp}/iso/");
    $self->{hiso}->buildiso("../output/","$self->{cfg}->{tmp}/iso/");

    $self->{hrain}->genupgrade("../output/") if( !(grep /^(repackiso)$/,@{$self->{args}})  );
}

sub help{
    my  $self = shift;
    print "ARGS: versionno   [sa] [noftp] [us] [testinst|updlist|updrepo|updlive|repackiso|updtemplate=newcentos.iso]\n" ;
    print   "\tsa:      is build for rainSA(default build for rain)\n";
    print   "\tnoftp:   don't download request files from remote FTP server\n";
    print   "\tus:      download request files from AWS build FTP server(default from NJ build FTP)\n";
    print   "\trepackiso[=isodir[,version]]:  recreate iso file from last build(only change build number\n";
    print   "\n";
    print   "\ttestinst: generate rpms.txt \n";
    print   "\tupdlist: update (rain or rainSA's) rpms.txt and rpms_local.txt\n";
    print   "\tupdrepo[=pkgname1,pkgname2...]: update (rain or rainSA's) CentOS repo from rpms.txt or pkgname(is specialfy)\n";
    print   "\n";
    print   "\tupdtemplate:  update (rain and rainSA's) (rain|sa)/anaconda/ and squashfs.img from ISO\n";
    print   "\tupdlive: update (rain or rainSA's) CentOS install UI(squashfs.img) from (rain|sa)/anaconda/ dir\n";
    print   "\n";
    print   "\n";
    exit 1;
}

#=================================================================================================
package main;

my $cfgname         = (grep /^sa$/,@ARGV) ? 'sa':'rain';
my $isus            = ( grep /^us$/,@ARGV);
my $version_number  = first {/^\d+$/} @ARGV;
my $version         = $version_number ? "6.0.$version_number" : '6.0.0001';

my $cmd             = new CmdCtl($cfgname,$isus,$version,@ARGV);
$cmd->exec();

