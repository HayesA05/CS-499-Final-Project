#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include "sqlite3.h"
#include <limits>

#define RESET   "\033[0m"
#define RED     "\033[31m"   // Errors
#define GREEN   "\033[32m"   // Success messages
#define BLUE    "\033[34m"   // Menu title / borders
#define CYAN    "\033[36m"   // Menu options
#define WHITE   "\033[37m"   // Default text

using namespace std;

bool isAdmin = false; //Tracks the admin login
const string ADMIN_USER = "admin";
const string ADMIN_PASS = "CS499";

// ==============================
// Course structure
// ==============================
// ENHANCED: Designed to directly map to a SQL database schema
// Courses table: courseNumber, courseTitle, major, category
// Prerequisites table: courseNumber, prerequisiteNumber
struct Course {
    string courseNumber;              // Primary key
    string courseTitle;               // Course name
    string major;                     // Academic major
    string category;                  // Course category
    vector<string> prerequisites;     // Related prerequisite courses
};

// ==============================
// Convert string to uppercase
// ==============================
// UNCHANGED: Ensures consistent comparisons and keys
string toUpper(const string& str) {
    string result = str;
    for (char& c : result) {
        c = toupper(c);
    }
    return result;
}

// ==============================
// Split string by delimiter
// ==============================
// UNCHANGED: Used for CSV parsing
vector<string> split(const string& line, char delimiter = ',') {
    vector<string> tokens;
    string token;
    istringstream tokenStream(line);

    while (getline(tokenStream, token, delimiter)) {
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        tokens.push_back(token);
    }
    return tokens;
}

void clearInput() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

//====================================================
//  Admin login function
//====================================================
void adminLogin(bool& isAdmin) {
    string user, pass;

    cout << "Username: ";
    cin >> user;
    cout << "Password: ";
    cin >> pass;

    if (user == ADMIN_USER && pass == ADMIN_PASS) {
        isAdmin = true;
        cout << GREEN << "Admin login successful.\n" << RESET;
    }
    else {
        cout << RED << "Invalid credentials.\n" << RESET;
    }
}

//====================================================
// Database helper
//====================================================
sqlite3* openDB() {
    sqlite3* db;
    if (sqlite3_open("sqlite/courses.db", &db) != SQLITE_OK) {
        cout << "DB open failed\n";
        return nullptr;
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    return db;
}


// ===================================================
// Load courses from CSV file (File-based data source)
// ===================================================
// EXISTING FUNCTIONALITY: Maintained for backward compatibility
// ENHANCED: Supports SQL-ready fields (major and category)
void loadCoursesFromCSV(const string& filename,
    unordered_map<string, Course>& courseTable) {

    courseTable.clear();
    ifstream file(filename);

    if (!file.is_open()) {
        cout << "Error opening file: " << filename << endl;
        return;
    }

    string line;
    while (getline(file, line)) {
        vector<string> tokens = split(line);

        // Format:
        // CourseNumber,Title,Major,Category,Prereq1,Prereq2,...
        if (tokens.size() < 4) {
            continue;
        }

        Course course;
        course.courseNumber = toUpper(tokens[0]);
        course.courseTitle = tokens[1];
        course.major = tokens[2];
        course.category = tokens[3];

        for (size_t i = 4; i < tokens.size(); ++i) {
            course.prerequisites.push_back(toUpper(tokens[i]));
        }

        courseTable[course.courseNumber] = course;
    }

    file.close();
    cout << "CSV data loaded successfully." << endl;
}

// ===================================================
// Load courses from SQL database (Database data source)
// ===================================================
// This function retrieves course and prerequisite
// data from a SQL database and populates courseTable
void loadCoursesFromDatabase(unordered_map<string, Course>& courseTable) {
    courseTable.clear();

    // 1. Open SQLite database
    sqlite3* db;
    int rc = sqlite3_open("sqlite/courses.db", &db);  // adjust path if needed

    if (rc) {
        cout << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }
    else {
        cout << "Opened database successfully." << endl;
    }

    char* errMsg = nullptr;

    // 2. Create Courses table if it doesn't exist
    const char* sqlCreateCourses =
        "CREATE TABLE IF NOT EXISTS Courses("
        "CourseNumber TEXT PRIMARY KEY NOT NULL,"
        "Title TEXT NOT NULL,"
        "Major TEXT,"
        "Category TEXT);";

    rc = sqlite3_exec(db, sqlCreateCourses, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cout << "SQL error (Courses table): " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    // 3. Create Prerequisites table if it doesn't exist
    const char* sqlCreatePrereqs =
        "CREATE TABLE IF NOT EXISTS Prerequisites("
        "CourseNumber TEXT NOT NULL,"
        "PrerequisiteNumber TEXT NOT NULL,"
        "PRIMARY KEY (CourseNumber, PrerequisiteNumber));";

    rc = sqlite3_exec(db, sqlCreatePrereqs, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cout << "SQL error (Prerequisites table): " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    // ==============================
    // Query Courses table
    // ==============================
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, "SELECT * FROM Courses;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cout << "Failed to fetch courses: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Course course;
        course.courseNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        course.courseTitle = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        course.major = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        course.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        courseTable[course.courseNumber] = course;
    }
    sqlite3_finalize(stmt);

    // ==============================
    // Query Prerequisites table
    // ==============================
    rc = sqlite3_prepare_v2(db, "SELECT * FROM Prerequisites;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cout << "Failed to fetch prerequisites: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string courseNum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string prereqNum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (courseTable.find(courseNum) != courseTable.end()) {
            courseTable[courseNum].prerequisites.push_back(prereqNum);
        }
    }
    sqlite3_finalize(stmt);

    cout << "Database loaded successfully." << endl;

    // 5. Close the database
    sqlite3_close(db);
}


// ==============================
// Print sorted list of courses
// ==============================
// UNCHANGED: Works regardless of data source
void printCourseList(const unordered_map<string, Course>& courseTable) {
    vector<string> keys;

    for (const auto& pair : courseTable) {
        keys.push_back(pair.first);
    }

    sort(keys.begin(), keys.end());

    cout << "Here is a sample schedule:" << endl;
    for (const string& key : keys) {
        const Course& course = courseTable.at(key);
        cout << course.courseNumber << ", " << course.courseTitle << endl;
    }
}

// ==============================
// Print course details
// ==============================
// ENHANCED: Displays SQL-ready fields
void printCourseDetails(const unordered_map<string, Course>& courseTable,
    const string& input) {

    string courseNumber = toUpper(input);
    auto it = courseTable.find(courseNumber);

    if (it == courseTable.end()) {
        cout << "Course not found." << endl;
        return;
    }

    const Course& course = it->second;

    cout << course.courseNumber << ", " << course.courseTitle << endl;
    cout << "Major: " << course.major << endl;
    cout << "Category: " << course.category << endl;

    if (course.prerequisites.empty()) {
        cout << "Prerequisites: None" << endl;
    }
    else {
        cout << "Prerequisites: ";
        for (size_t i = 0; i < course.prerequisites.size(); ++i) {
            cout << course.prerequisites[i];
            if (i < course.prerequisites.size() - 1) {
                cout << ", ";
            }
        }
        cout << endl;
    }
}

// ==============================
// Print courses by major
// ==============================
// ENHANCED: SQL-style filtering logic
void printCoursesByMajor(const unordered_map<string, Course>& courseTable,
    const string& major) {

    vector<string> keys;
    string searchMajor = toUpper(major);

    for (const auto& pair : courseTable) {
        if (toUpper(pair.second.major) == searchMajor) {
            keys.push_back(pair.first);
        }
    }

    if (keys.empty()) {
        cout << "No courses found for that major." << endl;
        return;
    }

    sort(keys.begin(), keys.end());

    cout << "Courses for major: " << major << endl;
    for (const string& key : keys) {
        const Course& course = courseTable.at(key);
        cout << course.courseNumber << ", " << course.courseTitle << endl;
    }
}


//===============================
// Add CREATE
//===============================
void addCourseDB() {
    if (!isAdmin) { cout << RED << "Admin only.\n" << RESET; return; }

    sqlite3* db = openDB();
    if (!db) return;

    string num, title, major, cat;
    cout << "Course Number: "; cin >> num;
    clearInput();
    cout << "Title: "; getline(cin, title);
    cout << "Major: "; getline(cin, major);
    cout << "Category: "; getline(cin, cat);

    const char* sql =
        "INSERT INTO Courses (CourseNumber, Title, Major, Category) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, num.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, major.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, cat.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        cout << "Course added.\n";
    else
        cout << "Insert failed.\n";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

//===============================
// Add UPDATE
//===============================
void updateCourseDB() {
    if (!isAdmin) { cout << RED << "Admin only.\n" << RESET; return; }

    sqlite3* db = openDB();
    if (!db) return;

    string num, title, major, cat;
    cout << "Course Number: "; cin >> num;
    clearInput();
    cout << "New Title: "; getline(cin, title);
    cout << "New Major: "; getline(cin, major);
    cout << "New Category: "; getline(cin, cat);

    const char* sql =
        "UPDATE Courses SET Title=?, Major=?, Category=? "
        "WHERE CourseNumber=?;";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, major.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cat.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, num.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);

    if (sqlite3_changes(db) > 0)
        cout << "Updated.\n";
    else
        cout << "Course not found.\n";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

//===============================
// Add DELETE
//===============================
void deleteCourseDB() {
    if(!isAdmin) { cout << RED << "Admin only.\n" << RESET; return; }

    sqlite3* db = openDB();
    if (!db) return;

    string num;
    cout << "Course Number: ";
    cin >> num;

    // Delete prerequisites first
    const char* sqlPrereq = "DELETE FROM Prerequisites WHERE CourseNumber=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sqlPrereq, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, num.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Delete course
    const char* sqlCourse = "DELETE FROM Courses WHERE CourseNumber=?;";
    sqlite3_prepare_v2(db, sqlCourse, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, num.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);

    if (sqlite3_changes(db) > 0)
        cout << "Deleted.\n";
    else
        cout << "Course not found.\n";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}


// ==============================
// Display menu
// ==============================
// ENHANCED: Explicit choice between CSV and SQL sources
void displayMenu() {

    cout << BLUE << "================ Course Planner Menu ================" << RESET << endl;
    cout << CYAN;
    cout << "1. Load Data Structure (CSV)." << endl;
    cout << "2. Print Course List." << endl;
    cout << "3. Print Course." << endl;
    cout << "4. Print Courses by Major." << endl;
    cout << "5. Load Data Structure (Database)." << endl;

    if (!isAdmin) {
        cout << "6. Admin Login" << endl;
    }
    else {
        cout << "6. Logout Admin" << endl;
        cout << "7. Add Course (Admin only)" << endl;
        cout << "8. Update Course (Admin only)" << endl;
        cout << "9. Delete Course (Admin only)" << endl;
    }

    cout << "10. Exit" << endl;
    

    cout << RESET;
    cout << BLUE << "===================================================" << RESET << endl;
}


// ==============================
// Main program
// ==============================
// ENHANCED: Supports multiple data sources
int main() {
    unordered_map<string, Course> courseTable;
    int choice;
    string input;
    bool dataLoaded = false;


    cout << "Welcome to the course planner." << endl;

    do {
        displayMenu();
        cout << "What would you like to do? ";
        cin >> input;

        try {
            choice = stoi(input);
        }
        catch (...) {
            choice = -1;
        }

        switch (choice) {
        case 1: {
            string filename;
            cout << "Enter the CSV file name: ";
            cin >> filename;
            loadCoursesFromCSV(filename, courseTable);
            dataLoaded = !courseTable.empty();
            break;
        }
        case 2:
            if (!dataLoaded) {
                cout << "Please load the data first." << endl;
            }
            else {
                printCourseList(courseTable);
            }
            break;
        case 3:
            if (!dataLoaded) {
                cout << "Please load the data first." << endl;
            }
            else {
                cout << "What course do you want to know about? ";
                cin >> input;
                printCourseDetails(courseTable, input);
            }
            break;
        case 4:
            if (!dataLoaded) {
                cout << "Please load the data first." << endl;
            }
            else {
                cout << "Enter the major: ";
                cin.ignore();
                getline(cin, input);
                printCoursesByMajor(courseTable, input);
            }
            break;
        case 5:
            loadCoursesFromDatabase(courseTable);
            dataLoaded = !courseTable.empty(); // mark data as loaded
            break;
            //ADMIN AND CRUD IMPLEMENTATION
        case 6:
            if (!isAdmin) {
                adminLogin(isAdmin); // login
            }
            else {
                isAdmin = false;     // logout
                cout << GREEN << "Admin logged out.\n" << RESET;
            }
            break;
        case 7:
            addCourseDB();
            break;
        case 8:
            updateCourseDB();
            break;
        case 9:
            deleteCourseDB();
            break;
        case 10:
            cout << "Thank you for using the course planner!" << endl;
            break;
        default:
            cout << input << " is not a valid option." << endl;
        }

    } while (choice != 10);

    return 0;
}
