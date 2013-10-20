use ExtUtils::MakeMaker;
WriteMakefile(
    NAME		=> 'Hash::Dynamic',
    VERSION_FROM	=> 'Dynamic.pm',
    OPTIMIZE		=> '-g',
    OBJECT		=> 'Dynamic.o libhx.a',
    EXE_FILES           => [glob("bin/*")],
    clean		=> { FILES => 'fork.hx one.hx two.hx' },
);
use MY::libscan;
use MY::ppd_install;
