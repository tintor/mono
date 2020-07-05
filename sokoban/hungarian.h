#include "core/matrix.h"
/**
 * An implementation of the O(n^3) Hungarian method for the minimum cost assignment problem
 * (maximum value matching can be computed by subtracting each value from the minimum value).
 *
 * It is assumed that the matrix is SQUARE. Code to ensure this could be easily added to the constructor.
 *
 * new Hungarian(costMatrix).execute() returns a 2d array,
 * with result[i][0] being the row index assigned to the result[i][1] column index (for assignment i).
 *
 * This method uses O(n^3) time (or at least, it should) and O(n^2) memory; it is
 * probably possible to reduce both computation and memory usage by constant factors using a few more tricks.
 */

struct Hungarian {
   public:
    matrix<int> costs;

   private:
    int dim;

    matrix<uchar> primes;
    matrix<uchar> stars;
    std::vector<uchar> rowsCovered;
    std::vector<uchar> colsCovered;
    std::vector<int> result;
    std::vector<int> primeLocations;
    std::vector<int> starLocations;

   public:
    // Note: costs must be range [0, std::numeric_limits<int>::max() / 2]
    Hungarian(int dim) : dim(dim) {
        if (dim > std::numeric_limits<short>::max()) THROW(invalid_argument);
        costs.resize(dim, dim);
        primes.resize(dim, dim);
        stars.resize(dim, dim);
        rowsCovered.resize(dim);
        colsCovered.resize(dim);
        result.resize(dim);
        primeLocations.resize(dim * 2);
        starLocations.resize(dim * 2);
    }

    static int makeLocation(int row, int col) { return (row << 16) | col; }

    static int rowFromLocation(int loc) { return loc >> 16; }

    static int colFromLocation(int loc) { return (loc << 16) >> 16; }

    std::vector<int>& execute() {
        resetPrimes();
        subtractRowColMins();
        findStars();             // O(n^2)
        resetCovered();          // O(n);
        coverStarredZeroCols();  // O(n^2)

        while (!allColsCovered()) {
            int primedLocation = primeUncoveredZero();  // O(n^2)

            // It's possible that we couldn't find a zero to prime, so we have to induce some zeros so we can find one
            // to prime
            if (primedLocation == makeLocation(-1, -1)) {
                minUncoveredRowsCols();                 // O(n^2)
                primedLocation = primeUncoveredZero();  // O(n^2)
            }

            // is there a starred 0 in the primed zeros row?
            int primedRow = rowFromLocation(primedLocation);
            int starCol = findStarColInRow(primedRow);
            if (starCol != -1) {
                // cover the row of the primedLocation and uncover the star column
                rowsCovered[primedRow] = true;
                colsCovered[starCol] = false;
            } else {  // otherwise we need to find an augmenting path and start over.
                augmentPathStartingAtPrime(primedLocation);
                resetCovered();
                resetPrimes();
                coverStarredZeroCols();
            }
        }

        return starsToAssignments();  // O(n^2)
    }

    // the starred 0's in each column are the assignments. O(n^2)
    std::vector<int>& starsToAssignments() {
        for (int j = 0; j < dim; j++) result[j] = findStarRowInCol(j);  // O(n)
        return result;
    }

    void resetPrimes() { primes.fill(false); }

    void resetCovered() {
        for (auto& e : rowsCovered) e = false;
        for (auto& e : colsCovered) e = false;
    }

    // get the first zero in each column, star it if there isn't already a star in that row
    // cover the row and column of the star made, and continue to the next column. O(n^2)
    void findStars() {
        resetCovered();
        stars.fill(false);

        for (int j = 0; j < dim; j++) {
            for (int i = 0; i < dim; i++)
                if (costs(i, j) == 0 && !rowsCovered[i] && !colsCovered[j]) {
                    stars(i, j) = true;
                    rowsCovered[i] = true;
                    colsCovered[j] = true;
                    break;
                }
        }
    }

   private:
    /*
     * Finds the minimum uncovered value, and adds it to all the covered rows then
     * subtracts it from all the uncovered columns. This results in a cost matrix with
     * at least one more zero.
     */
    void minUncoveredRowsCols() {
        // find min uncovered value
        int minUncovered = std::numeric_limits<int>::max();
        for (int i = 0; i < dim; i++)
            if (!rowsCovered[i])
                for (int j = 0; j < dim; j++)
                    if (!colsCovered[j])
                        if (costs(i, j) < minUncovered) minUncovered = costs(i, j);

        // add that value to all the COVERED rows.
        for (int i = 0; i < dim; i++)
            if (rowsCovered[i])
                for (int j = 0; j < dim; j++)
                    if (costs(i, j) + minUncovered < costs(i, j))
                        THROW(runtime_error, "%s %s", costs(i, j), minUncovered);

        for (int i = 0; i < dim; i++)
            if (rowsCovered[i])
                for (int j = 0; j < dim; j++) costs(i, j) += minUncovered;

        // subtract that value from all the UNcovered columns
        for (int j = 0; j < dim; j++)
            if (!colsCovered[j])
                for (int i = 0; i < dim; i++) costs(i, j) -= minUncovered;
    }

    /*
     * Finds an uncovered zero, primes it, and returns an array
     * describing the row and column of the newly primed zero.
     * If no uncovered zero could be found, returns -1 in the indices.
     * O(n^2)
     */
    int primeUncoveredZero() {
        for (int i = 0; i < dim; i++)
            if (!rowsCovered[i])
                for (int j = 0; j < dim; j++)
                    if (!colsCovered[j]) {
                        if (costs(i, j) == 0) {
                            primes(i, j) = true;
                            return makeLocation(i, j);
                        }
                    }
        return makeLocation(-1, -1);
    }

    /*
     * Starting at a given primed location[0=row,1=col], we find an augmenting path
     * consisting of a primed , starred , primed , ..., primed. (note that it begins and ends with a prime)
     * We do this by starting at the location, going to a starred zero in the same column, then going to a primed zero
     * in the same row, etc, until we get to a prime with no star in the column. O(n^2)
     */
    void augmentPathStartingAtPrime(int location) {
        int primeLocationsSize = 0;
        int starLocationsSize = 0;
        primeLocations[primeLocationsSize++] = location;

        int currentRow = rowFromLocation(location);
        int currentCol = colFromLocation(location);
        while (true) {  // add stars and primes in pairs
            int starRow = findStarRowInCol(currentCol);
            // at some point we won't be able to find a star. if this is the case, break.
            if (starRow == -1) break;
            starLocations[starLocationsSize++] = makeLocation(starRow, currentCol);
            currentRow = starRow;

            int primeCol = findPrimeColInRow(currentRow);
            primeLocations[primeLocationsSize++] = makeLocation(currentRow, primeCol);
            currentCol = primeCol;
        }

        unStarLocations(starLocations, starLocationsSize);
        doStarLocations(primeLocations, primeLocationsSize);
    }

    void doStarLocations(std::vector<int>& locations, int size) {
        for (int k = 0; k < size; k++) {
            int row = rowFromLocation(locations[k]);
            int col = colFromLocation(locations[k]);
            stars(row, col) = true;
        }
    }

    void unStarLocations(std::vector<int>& locations, int size) {
        for (int k = 0; k < size; k++) {
            int row = rowFromLocation(locations[k]);
            int col = colFromLocation(locations[k]);
            stars(row, col) = false;
        }
    }

    // Given a row index, finds a column with a prime. returns -1 if this isn't possible
    int findPrimeColInRow(int row) {
        for (int j = 0; j < dim; j++)
            if (primes(row, j)) return j;
        return -1;
    }

    // Given a column index, finds a row with a star. returns -1 if this isn't possible
    int findStarRowInCol(int col) {
        for (int i = 0; i < dim; i++)
            if (stars(i, col)) return i;
        return -1;
    }

    int findStarColInRow(int row) {
        for (int j = 0; j < dim; j++)
            if (stars(row, j)) return j;
        return -1;
    }

    // looks at the colsCovered array, and returns true if all entries are true, false otherwise
    bool allColsCovered() {
        for (int j = 0; j < dim; j++)
            if (!colsCovered[j]) return false;
        return true;
    }

    // sets the columns covered if they contain starred zeros O(n^2)
    void coverStarredZeroCols() {
        for (int j = 0; j < dim; j++) {
            bool covered = false;
            for (int i = 0; i < dim; i++)
                if (stars(i, j)) {
                    covered = true;
                    break;
                }
            colsCovered[j] = covered;
        }
    }

    void subtractRowColMins() {
        for (int i = 0; i < dim; i++) {  // for each row
            int rowMin = std::numeric_limits<int>::max();
            for (int j = 0; j < dim; j++)  // grab the smallest element in that row
                if (costs(i, j) < rowMin) rowMin = costs(i, j);
            for (int j = 0; j < dim; j++)  // subtract that from each element
                costs(i, j) -= rowMin;
        }

        for (int j = 0; j < dim; j++) {
            int colMin = std::numeric_limits<int>::max();
            for (int i = 0; i < dim; i++)  // grab the smallest element in that column
                if (costs(i, j) < colMin) colMin = costs(i, j);
            for (int i = 0; i < dim; i++)  // subtract that from each element
                costs(i, j) -= colMin;
        }
    }
};
