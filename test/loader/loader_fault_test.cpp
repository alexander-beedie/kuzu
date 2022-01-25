#include "test/test_utility/include/test_helper.h"

#include "src/common/include/exception.h"

using namespace graphflow::testing;
using namespace std;

class LoaderFaultTest : public BaseGraphLoadingTest {
public:
    void SetUp() override{};

    void checkLoadingFaultWithErrMsg(string expectedErrorMsg) {
        try {
            loadGraph();
            FAIL();
        } catch (LoaderException& e) {
            auto errmsg = string(e.what());
            if (errmsg.find(expectedErrorMsg) == string::npos) {
                FAIL();
            }
        } catch (Exception& e) { FAIL(); }
    }

private:
    void loadGraph() {
        testSuiteSystemConfig.graphInputDir = getInputCSVDir();
        testSuiteSystemConfig.graphOutputDir = TEMP_TEST_DIR;
        TestHelper::loadGraph(testSuiteSystemConfig);
    }
};

class LoaderLongStringTest : public LoaderFaultTest {
    string getInputCSVDir() override { return "dataset/loader-fault-tests/long-string/"; }
};

class LoaderNodeFileNoIDFieldTest : public LoaderFaultTest {
    string getInputCSVDir() override { return "dataset/loader-fault-tests/node-file-no-ID/"; }
};

class LoaderNodeFileWrongFieldTest : public LoaderFaultTest {
    string getInputCSVDir() override { return "dataset/loader-fault-tests/node-file-wrong-field/"; }
};

class LoaderRelFileNoMandatoryFieldsTest : public LoaderFaultTest {
    string getInputCSVDir() override {
        return "dataset/loader-fault-tests/rel-file-no-mandatory-fields/";
    }
};

class LoaderRelFileWrongFieldTest : public LoaderFaultTest {
    string getInputCSVDir() override { return "dataset/loader-fault-tests/rel-file-wrong-field/"; }
};

class LoaderImproperMandatoryFieldColHeaderTest : public LoaderFaultTest {
    string getInputCSVDir() override {
        return "dataset/loader-fault-tests/improper-mandatory-field-col-header/";
    }
};

class LoaderImproperPropertyColHeaderTest : public LoaderFaultTest {
    string getInputCSVDir() override {
        return "dataset/loader-fault-tests/improper-property-col-header/";
    }
};

class LoaderDuplicateColHeaderTest : public LoaderFaultTest {
    string getInputCSVDir() override { return "dataset/loader-fault-tests/duplicate-col-header/"; }
};

TEST_F(LoaderLongStringTest, LongStringError) {
    checkLoadingFaultWithErrMsg(
        "Maximum length of strings is 4096. Input string's length is 5625.");
}

TEST_F(LoaderNodeFileNoIDFieldTest, NodeFileNoIDFieldError) {
    checkLoadingFaultWithErrMsg(
        "Column header definitions of a node file does not contains the mandatory field `ID`.");
}

TEST_F(LoaderNodeFileWrongFieldTest, NodeFileWrongFieldError) {
    checkLoadingFaultWithErrMsg("Column headers definitions of a node file contains a mandatory "
                                "field `START_ID` that is not allowed.");
}

TEST_F(LoaderRelFileNoMandatoryFieldsTest, RelFileNoMandatoryFieldsError) {
    checkLoadingFaultWithErrMsg(
        "Column header definitions of a rel file does not contains all the mandatory field.");
}

TEST_F(LoaderRelFileWrongFieldTest, RelFileWrongFieldError) {
    checkLoadingFaultWithErrMsg(
        "Column header definitions of a rel file cannot contain the mandatory field `ID`.");
}

TEST_F(LoaderImproperMandatoryFieldColHeaderTest, ImproperMandatoryFieldColHeaderError) {
    checkLoadingFaultWithErrMsg("Invalid mandatory field column header `IDS`.");
}

TEST_F(LoaderImproperPropertyColHeaderTest, ImproperPropertyColHeaderError) {
    checkLoadingFaultWithErrMsg("Incomplete column header `fName`.");
}

TEST_F(LoaderDuplicateColHeaderTest, DuplicateColHeaderError) {
    checkLoadingFaultWithErrMsg("Column fName already appears previously in the column headers.");
}