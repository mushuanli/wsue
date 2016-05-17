#!/usr/bin/perl

use strict;

my  $infilenameprefix='aa.';
my  $outfilenameprefix='bb.';
my  $filtercmd='| grep -vw clock_gettime  | grep -vw futex | grep -vw getuid32 | grep -vw gettimeofday | grep -vw mmap2 | grep -vw mprotect | grep -vw madvise | grep -vw rt_sigprocmask | grep -vw munmap ';

print "split_tree.pl pstree_file aa_file_path\n";
print "\tuse to parse this command:\n";
print "\t\t".'(/opt/bin/sniff -i eth0 -F \'TCP{PORT!389}\' -w 050413.cap  &); strace -s 300 -f -ff -o aa -tt  -p \"`pidof libvirtd`\";pkill -SIGINT sniff ; pstree -p >tree'."\n";

die "aa_file_path not found \n" if (! -d $ARGV[1]);


my  $pids   = pids_loadfromtree($ARGV[0]);

pids_dump($pids);
parse_pidfiles($pids,$ARGV[1],$ARGV[1]);

sub parse_onepidfile{
    my  ($pid,$fdir,$fout)  = @_;
    my  $infname    = "${fdir}/${infilenameprefix}$pid";

    my  $fin;

    open ($fin,'<',$infname) or die "open file <$infname> fail\n";

    while( my $line=<$fin>){
        $line =~ s/BT819_FIFO_RESET_HIGH/BINDER_WRITE_READ/g;
        if( $line =~ m/^(\d+:\d+:\d+\.\d+\s+)(.*)/ ){
            print $fout    "$1<$pid>: $2\n";
        }
        else{
            print $fout    "<$pid>: $line\n";
        }
    }

    close $fin;
}

sub parse_one_proc{
    my  ($name,$pidref,$indir,$outdir)    = @_;

    my  $outfname   = "${outdir}/${outfilenameprefix}${name}.txt";
    open (my $fout,'>',$outfname) or die "open file <$outfname> fail\n";

    foreach my $pid (@$pidref){
        parse_onepidfile($pid,$indir,$fout);
    }
    
    close $fout;
    rename($outfname,"${outfname}.tmp");
    system ("cat \"${outfname}.tmp\" $filtercmd | sort > \"$outfname\" ; rm -f \"${outfname}.tmp\" ");
}

sub parse_pidfiles{
    my  ($pidref,$indir,$outdir)    = @_;
    my  %pids   = %{$pidref};
    foreach my $key (keys %pids){
        parse_one_proc($key,$pids{$key},$indir,$outdir);
    }
}

sub pids_dump{
    my  %pids   = %{$_[0]};
    foreach my $key (keys %pids){
        my  @arr    = @{$pids{$key}};
        print "$key - @arr\n";
    }
}

sub pids_loadfromtree{
    my  $fname  = shift;
    my   $fh;
    open ($fh,'<',$fname) or die "open pstree_file <$fname> fail\n";


    my  %procpidinfo;

    my  $keyword_mode       = 0;    #   1: find libvirt_lxc 2: find libvirtd
    my  $keyword_treepos    = 0;
    my  $subkeyword_name;    
    my  $subkeyword_treepos = 0;    

    print "open file<$fname> succ, begin read\n";
    while(my $line = <$fh> ){
        my  $firstfind  = 0;

        if( $keyword_mode == 0 ){
            next if($keyword_mode == 0 && ($line !~m/(libvirt_lxc|libvirtd)/));

            if( $1 eq 'libvirt_lxc' ){
                $keyword_mode               = 1;
            }
            else{
                $keyword_mode               = 2;
            }

            $firstfind  = 1;
        }


        #   如果不是第一行，再判断是否是另一个关键字的开始
        if( !$firstfind ){
            my  $firstpart  = substr($line,0,$keyword_treepos);
            if( $firstpart =~ m/\w/ ){
                if( $line =~ m/(libvirt_lxc|libvirtd)/ ){
                    my  $match = $1;
                    if( $keyword_mode == 2 && ($match eq 'libvirt_lxc') ){
                        $keyword_mode           = 1;
                        $firstfind              = 1;
                    }
                    else{
                        if( $keyword_mode == 1 && ($match eq 'libvirtd') ){
                            $keyword_mode       = 2;
                            $firstfind          = 1;
                        }
                    }
                }

                if( !$firstfind){
                    $keyword_mode   = 0;
                    next;
                }
            }
        }

        my  $key;
        my  @words  = split(/-+/,$line);
        shift @words while( $words[0] !~ m/\(\d+\)/ );  #   skip not match line


        if( $firstfind ){
            $keyword_treepos    = index($line,'+');
            undef $subkeyword_name;    
            $subkeyword_treepos = 0;    

            if( $keyword_mode == 1 ){
                my  $subword    = 0;
                foreach my $word (@words){
                    if( $word eq '+' ){
                        $subword    = 1;
                        next;
                    }

                    next if( $word !~ m/(\{?.+\}?)\((\d+)\)/);
                    if( $subword < 2 ){
                        $key        = $1;
                        $subword ++ if($subword == 1);
                    }

                    my  @oldarr;
                    my  $pid        = $2;
                    if($procpidinfo{$key}){
                        @oldarr = @{$procpidinfo{$key}} ;
                    }
                    else{
                        undef @oldarr;
                    }
                    push @oldarr,$pid;
                    $procpidinfo{$key}  = \@oldarr;
                    #print "$key - @oldarr\n";
                }

                $keyword_mode   = 0 if( $keyword_treepos <= 0 );
                next;
            }
        }

        if( $keyword_mode == 2 ){
            my  @oldarr;
            if($procpidinfo{libvirtd}){
                @oldarr = @{$procpidinfo{libvirtd}} ;
            }
            else{
                undef @oldarr;
            }

            foreach my $word (@words){
                next if( $word !~ m/\{?.+\}?\((\d+)\)/);
                push @oldarr,$1;
            }
            $procpidinfo{libvirtd}  = \@oldarr;

            #print "libvirtd - @oldarr\n";

            $keyword_mode   = 0 if( $keyword_treepos <= 0 );
            next;
        }

        #   keyword_mode    == 1 && ! $firstfind
        my  @oldarr;
        undef $key;
        next if($words[0] !~ m/(\{?.+\}?)\((\d+)\)/);
        $key=$1;

        if( $subkeyword_treepos <= 0 ){
            $subkeyword_treepos         = index($line,'+');
            $subkeyword_name            = $key;
        }
        else{
            my  $firstpart                  = substr($line,0,$subkeyword_treepos);
            if( $firstpart =~ m/\w/ ){
                #print "-$subkeyword_treepos-$firstpart:$line";
                $subkeyword_treepos         = index($line,'+');
                if( $subkeyword_treepos > 0 ){
                    $subkeyword_name    = $key;
                }
            }
            else{
                $key    = $subkeyword_name;
            }
        }

        if($procpidinfo{$key}){
            @oldarr = @{$procpidinfo{$key}} ;
        }
        else{
            undef @oldarr;
        }

        foreach my $word (@words){
            next if( $word !~ m/\{?.+\}?\((\d+)\)/);
            push @oldarr,$1;
        }

        $procpidinfo{$key}  = \@oldarr;
        #print "$key - @oldarr\n";

        #print "word is<$keyword_mode>: @words\n";
    }

    close $fh;

    #print %procpidinfo;

    return \%procpidinfo;
}
