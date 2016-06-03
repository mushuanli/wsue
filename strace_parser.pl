#!/usr/bin/perl

#   author: mushuanli@163.com/lizlok@gmail.com, a tool use to parse strace output
#   normally parse this command output:
#       '(/opt/bin/sniff -i eth0 -F \'TCP{PORT!389}\' -w 050413.cap  &); strace -s 300 -f -ff -o aa -tt  -p \"`pidof libvirtd`\";pkill -SIGINT sniff ; pstree -p >tree'
#   the program will:
#   1.  parse ARGV[0] file (parsetree_exec) to generate a {$procname,\@pids} hash.current the procname only include libvirtd and libvirt_lxc's child process(@config_parentprocname)
#   2.  merge process's all task trace output into single file and sort it
#   3.  convert file handle to filename, and put it to "$ARGV[1]/fd"
#   4.  use clean command to filter "$ARGV[1]/fd"'s all file, and output to "$ARGV[1]/fd/clear/"
use Cwd;
use strict;


#   
#   configure
#
#   1.  only trace these process and it's child process
my  @config_parentprocname=('libvirtd','libvirt_lxc');

#   2.  strace filename format, in '$config_pidfilenameprefix<pid>'
my  $config_pidfilenameprefix='aa.';
#   3.  will merge one process's all tid trace file into single file, filename format:
#       $config_outfilenameprefix<procname>
my  $config_outfilenameprefix='bb.';
my  $cleancmd=q{ egrep '(sendto|send|recv|recvfrom|read|write|connect|bind|listen|accept|execve|fork|vfork|clone)\\(' | grep -vw tasks | grep -vw binder };


my $pstreefile  = $ARGV[0] || 'tree';
my $stracedir   = $ARGV[1] || '.';
#   
#   help
#
print "strace_parse.pl pstree_file aa_file_path\n";
print "\tuse to parse this command:\n";
print "\t\t".'(/opt/bin/sniff -i eth0 -F \'TCP{PORT!389}\' -w 050413.cap  &); strace -s 300 -f -ff -o aa -tt  -p \"`pidof libvirtd`\";pkill -SIGINT sniff ; pstree -p >tree'."\n";

die "aa_file_path not found \n" if (! -d $stracedir);
mkdir "$stracedir/fd";            #   fd convert dir
mkdir "$stracedir/fd/clear/";     #   clean dir

my  $pids   = parsetree_exec($pstreefile);

parsetree_dump($pids);
my  @outfiles   = mergefile_exec($pids,$stracedir,$stracedir);
replacefd_exec( @outfiles);
generate_summary("$stracedir/fd/clear/");
















sub replacefd_exec{
    my  @files  = @_;
    foreach my $fname (@files){
        replacefd_1file($fname);
    }
}

sub replacefd_1file{
    my  $fname      = shift ;

    my  $foutname   = $fname;
    $foutname       =~ s/(.*)(\\|\/)(.*?)\.txt$/\1\/fd\/\3\.txt/g;
    my  $cleandir   = $fname;
    $cleandir      =~ s/(.*)(\\|\/)(.*?)\.txt$/\1\/fd\/clear\//g;
    my  $cleanname  = $fname;
    $cleanname      =~ s/(.*)(\\|\/)(.*?)\.txt$/$cleandir\3\.txt/g;

    my  @fds;
    #   print   "$fname -> $foutname\n";

    open (my $fin,'<',$fname) or die "open file $fname to parse fd fail\n";
    open (my $fout,'>',$foutname) or die "open file $foutname to write fd fail\n";
    while( my $line = <$fin> ){
        if( $line !~ m/^(\d+:\d+:\d+\.\d+\s+\<\d+\#\w+\>:\s+)(\w*(open|read|write|bind|connect|send|recv|ioctl|close|getpeername)\w*)\((.*)/){
            print   $fout   "$line";
            next;
        }

        my  $title  = $1;
        my  $func   = $2;
        my  $key    = $3;
        my  $rest   = $4;

        if( $func =~ m/(read|write|send|recv|ioctl)/ ){
            if( $rest =~ m/^(\d+),(.*)/ ){
                my  $fd         = $1;
                my  $funcrest   = $2;
                if( $fds[$fd] ){
                    print   $fout   "$title$func\($fd<$fds[$fd]>,$funcrest\n";
                    next;
                }
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/close/ ){
            if( $rest =~ m/^(\d+)\)(.*)/ ){
                my  $fd         = $1;
                my  $funcrest   = $2;
                if( $fds[$fd] ){
                    print   $fout   "$title$func\($fd<$fds[$fd]>\)$funcrest\n";
                    undef $fds[$fd];
                    next;
                }
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        #open|bind|connect|getpeername
        if($func eq 'openat' ){
            #openat(AT_FDCWD, "/proc/self/task", O_RDONLY|O_LARGEFILE|O_DIRECTORY) = 11
            if( $rest =~ m/^\w+, \"[^"]+(\\|\/)([^"]+)\".*= (\d+)$/ ){
                my  $fd     = $3;
                my  $name   = $2;

                print "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );

                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/(bind|connect)/ ){
            if( $rest =~ m/^(\d+),.*sun_path=\"[^"]+(\\|\/)([^"]+)\"/ ){
                my  $fd     = $1;
                my  $name   = $3;
                die "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );

                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/getpeername/ ){
            if( $rest =~ m/^(\d+),.*sin_port=htons\((\d+)\), sin_addr=inet_addr\(\"([0-9.]+)\"\)/ ){
                my  $fd     = $1;
                my  $name   = "$3:$2";

                die "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );
                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        #print "$title   - $func - $rest \n";
    }

    close $fin;
    close $fout;

    system( "head -n1 $foutname > $cleanname");
    system( " cat $foutname |$cleancmd >> $cleanname");
}


sub generate_summary{
    my  ($dir)  = @_;
    my $curdir=getcwd();
    chdir($dir);
    
#   generate property_service time
    unlink("property_service");
    unlink("process.tmp");
    unlink("index");

    my $cmd=q{egrep 'sendto\([0-9]+<property_service>,' bb.* \
    | sed -e 's/sendto([0-9]\+<property_service>, \"\\\\[0-9]\\\\0\\\\0\\\\0/    /' \
    | sed -e 's/\\\\0/ /g'| sed -e 's/\".*//' \
    | sed -e 's/^bb\.\(.*\)_[0-9]\+\.txt:\([0-9]\+:[0-9]\+:[0-9]\+\.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3    /' | sort > property_service};
    print "$cmd\n";
    qx($cmd);

    #   generate process execve/bind/listen/accept time
    $cmd=q{egrep -w 'vfork|fork|clone' bb.* | grep -v '|CLONE_THREAD|' \
    | sed -e 's/^bb\.\(.*\)_[0-9]\+\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>: \([a-z]\+\).*= \([0-9]\+\)/\2 \1:\3  \4  = \5/' > process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{egrep -w 'listen|accept' bb.* \
    | sed -e 's/^bb\.\(.*\)_[0-9]\+\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>: \([a-z]\+\)(\([0-9]\+\), .*= \([0-9]\+\)/\2 \1:\3  \4   \5/' >> process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{egrep -w 'bind' bb.* | sed -e 's/bind(\([0-9]\+\), /bind \1 /'  \
    | sed -e 's/\{sa_family=AF_INET6, sin6_port=htons(\([0-9]\+\)),.* = \([0-9]\+\)/ INET6 \1/' \
    | sed -e 's/\{sa_family=.*\"\(.*\)\".* = \([0-9]\+\)/\1/' \
    | sed -e 's/\{sa_family=AF_NETLINK, pid=\([0-9]\+\), groups=\([0-9a-zA-Z]\+\).* = \([0-9]\+\)/ NETLINK \1,\2/g' \
    | sed -e 's/^bb\.\(.*\)_[0-9]\+\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3 /' >> process.tmp};
    print "$cmd\n";
    qx($cmd);
    
    $cmd=q{egrep -w execve bb.*  \
    | sed -e 's/ execve(\"\([^"]\+\)\".*/    EXEC \1/' \
    | sed -e 's/^bb\.\(.*\)_[0-9]\+\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3/' >> process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{cat property_service process.tmp \
    | sort  |awk ' { printf "%s\\t%-20s\\t%-10s\\t%s\\t%s\\t%s\\t%s\\t%s\\n",$1,$2,$3,$4,$5,$6,$7,$8}' > index };
    print "$cmd\n";
    qx($cmd);
    unlink("process.tmp");
    chdir ($curdir);
}










sub mergefile_1tsk{
    my  ($pid,$thridx,$fdir,$fout)  = @_;
    my  $infname    = "${fdir}/${config_pidfilenameprefix}$pid";

    my  $fin;
    open ($fin,'<',$infname) or die "open file <$infname> fail\n";

    my  $alias  = '#'."$thridx";
    $alias      = '#main' if( $thridx == 0 );

    while( my $line=<$fin>){
        $line =~ s/BT819_FIFO_RESET_HIGH/BINDER_WRITE_READ/g;
        if( $line =~ m/^(\d+:\d+:\d+\.\d+\s+)(.*)/ ){
            print $fout    "$1<$pid$alias>: $2\n";
        }
        else{
            print $fout    "<$pid$alias>: $line\n";
        }
    }

    close $fin;
}

sub mergefile_1proc{
    my  ($name,$pidref,$indir,$outdir)    = @_;

    my  $outfname   = "${outdir}/${config_outfilenameprefix}${name}.txt";
    open (my $fout,'>',$outfname) or die "open file <$outfname> fail\n";
    
    my  $i      = 0;
    my  @pids   = @$pidref;
    my  $pid    = shift @pids;
    @pids       = sort @pids;
    unshift @pids,$pid;

    foreach my $pid (@pids){
        mergefile_1tsk($pid,$i,$indir,$fout);
        $i++;
    }
    
    close $fout;
    rename($outfname,"${outfname}.tmp");
    system ("cat \"${outfname}.tmp\" | sort > \"$outfname\" ; rm -f \"${outfname}.tmp\" ");
    return $outfname;
}

sub mergefile_exec{
    my  ($pidref,$indir,$outdir)    = @_;
    my  %pids   = %{$pidref};

    my  @alloutfiles;
    foreach my $key (keys %pids){
        my  $outfile    = mergefile_1proc($key,$pids{$key},$indir,$outdir);
        push    @alloutfiles,$outfile;
    }

    return @alloutfiles;
}





sub parsetree_dump{
    my  %pids   = %{$_[0]};
    foreach my $key (keys %pids){
        my  @arr    = @{$pids{$key}};
        print "$key - @arr\n";
    }
}

#   simple  mode, use hash array:
#       name    item    level   lineno

sub parsetree_analyselinepart{
    my  ($level,$resref,$lineno,$line,$offset,$end)    = @_;

    my  $part;

    if( $end == -1 ){
        $part       = substr($line,$offset);
    }
    else{
        $part       = substr($line,$offset, $end-$offset);
    }

    $part =~ s/(\w+)-(\w+)/\1_\2/g;
    my  @words  = split(/-+/,$part);
    my  $olditem;

    foreach my $word (@words){
        next if( $word !~ m/((\{?).+\}?)\((\d+)\)/);

        my  $item       = {};
        $item->{name}   = $1;
        $item->{pid}    = $3;
        if( $2 eq '{' && $olditem ){
            $item->{level}  = $level+1;
        }
        else{
            $item->{level}  = $level;
        }
        $item->{lineno} = $lineno;
        #print "$item->{lineno} - $item->{name} - $item->{pid} - $item->{level} \n";
        push @$resref,$item;
        $olditem    = $item;
    }
}

sub parsetree_analyseline{
    my  ($lineno,$line,$resref,$stackref)  = @_;

    my  $stacksz    = scalar(@$stackref);
    my  $i          = $stacksz -1;

    #   parse stack first,
    #   if any character before stack[i], then means this item invalid
    for( ; $i > -1; $i -- ){
        my  $firstpart  = substr($line,0,$$stackref[$i]);
        if( $firstpart =~ m/\w/ ){
            pop @$stackref;
        }
        else{
            last;
        }
    }

    $i++;

    my $offset = 0;
    my $result = index($line, '+', $offset);
    while ($result != -1) {
        parsetree_analyselinepart($i,$resref,$lineno,$line,$offset,$result);

        $i++;
        push @$stackref,$result;

        $offset = $result + 1;
        $result = index($line, '+', $offset);
    }
    parsetree_analyselinepart($i,$resref,$lineno,$line,$offset,$result);
}


sub parsetree_pushpid{
    my  ($out,$name,$item)    = @_;

    print "push $name,$item->{pid}\n";
    if( !$out->{$name} ){
        my  @arr    = $item->{pid};
        $out->{$name} = \@arr;
    }
    else{
        push @{$out->{$name}},$item->{pid};
    }

}

sub parsetree_exec{
    my  $fname  = shift;
    my   $fh;
    open ($fh,'<',$fname) or die "open pstree_file <$fname> fail\n";


    my  %procpidinfo;



    my  @res;
    my  $i  = 1;

    my  @tmpstack;
    while(my $line = <$fh> ){
        parsetree_analyseline($i++,$line,\@res,\@tmpstack);
    }
    close $fh;

    my  $keyword;
    my  $start_line         = 0;
    my  $start_level        = 0;

    undef   @tmpstack;
    my  @namearray;
    my  @keepstat;


    foreach my $pidinfo (@res) {
        if( index($pidinfo->{name},'{') == 0 ){
            $keyword    = $namearray[$pidinfo->{level} - 1];
            parsetree_pushpid(\%procpidinfo,$keyword,$pidinfo) if($keepstat[$pidinfo->{level} - 1]);
            next;
        }

        $keyword    = "$pidinfo->{name}_$pidinfo->{pid}";
        if( $keyword ne $namearray[$pidinfo->{level}] ){
            $namearray[$pidinfo->{level}]   = "$pidinfo->{name}_$pidinfo->{pid}";
            if( -f "${config_pidfilenameprefix}$pidinfo->{pid}" ){
                $keepstat[$pidinfo->{level}]    = 1;
            }
            else{
                $keepstat[$pidinfo->{level}]    = 0;
            }
        }

        print "parse line : $pidinfo->{level} - $pidinfo->{name} - $pidinfo->{pid} - $keepstat[$pidinfo->{level}] ${config_pidfilenameprefix}$pidinfo->{pid}\n";
        parsetree_pushpid(\%procpidinfo,$keyword,$pidinfo) if($keepstat[$pidinfo->{level}]);
    }

    return \%procpidinfo;
}

