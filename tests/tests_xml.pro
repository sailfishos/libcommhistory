TEMPLATE = aux

OTHER_FILES += $$PWD/ performance_tests.xml.in $$PWD/run_test.sh

RUN_TEST=/opt/tests/libcommhistory-qt5/run_test.sh


run_test_install.files = $$PWD/run_test.sh
run_test_install.path = /opt/tests/libcommhistory-qt5


unit_xml.target = tests.xml
unit_xml.depends = $$PWD/tests.xml.in
unit_xml.commands = sed -e "s:@RUN_TEST@:$${RUN_TEST}:g" $< > $@
unit_xml.CONFIG += no_check_exist

unit_tests_install.path = /opt/tests/libcommhistory-qt5/auto
unit_tests_install.files = $$unit_xml.target
unit_tests_install.depends = unit_xml
unit_tests_install.CONFIG += no_check_exist


perf_xml.target = performance_tests.xml
perf_xml.depends = $$PWD/performance_tests.xml.in
perf_xml.commands = sed -e "s:@RUN_TEST@:$${RUN_TEST}:g" $< > $@
perf_xml.CONFIG += no_check_exist

perf_tests_install.path = /opt/tests/libcommhistory-qt5/performance
perf_tests_install.files = $$perf_xml.target
perf_tests_install.depends = perf_xml
perf_tests_install.CONFIG += no_check_exist


QMAKE_EXTRA_TARGETS = unit_xml perf_xml
QMAKE_CLEAN += $$unit_xml.target $$perf_xml.target
PRE_TARGETDEPS += $$unit_xml.target $$perf_xml.target

INSTALLS += run_test_install unit_tests_install perf_tests_install
